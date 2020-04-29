#pragma once

#include "executor.h"
#include "task_name.h"

#include <future>
#include <type_traits>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <iostream>

namespace tasks_helpers {
    template<typename t>
    inline auto wait_for_tasks(t &&task){
        return std::make_tuple(task->get_future().get());
    }

    template<typename t, typename... tasks_t>
    inline auto wait_for_tasks(t &&task, tasks_t&& ...tasks){
        return std::tuple_cat(
            wait_for_tasks(std::forward<t>(task)),
            wait_for_tasks(std::forward<tasks_t...>(tasks...)));
    }

    template<typename ...tasks_t>
    inline auto wait_for_tasks(std::tuple<tasks_t...> &tasks){
        return std::apply([](auto &&... args){
            return wait_for_tasks(args...);
        }, tasks);
    }


    template<typename t>
    inline void schedule_tasks(executor &exec, t &&task){
        return exec.schedule(task);
    }

    template<typename t, typename ...tasks_t>
    inline void schedule_tasks(executor &exec, t &&task, tasks_t&& ...tasks){
        schedule_tasks(exec, task);
        schedule_tasks(exec, tasks...);
    }

    template<typename ...tasks_t>
    inline void schedule_tasks(executor &exec, std::tuple<tasks_t...> &tasks){
        return std::apply([&exec](auto &&... tasks){
            schedule_tasks(exec, tasks...);
        }, tasks);
    }
}
// A few concepts to simplify templates
template<typename F> // function with no arguments
concept NullaryFunction = std::is_invocable<F>::value;

template<typename F, typename ArgumentType> // function with 1 argument
concept UnaryFunction = std::is_invocable<F, ArgumentType>::value;

// std::promise requires copy or move constructor, but also supports reference and void
template<typename T>
concept SupportsPromise = 
    std::is_copy_constructible<T>::value ||
    std::is_move_constructible<T>::value ||
    std::is_same<T, void>::value ||
    std::is_reference<T>::value;

// Generic asynchronous task returning result
template<SupportsPromise result>
class task : public executable {
    public:
    using result_t = result;

    template<NullaryFunction fn_t>
    explicit task(executor &ex, fn_t &&fn)
        : fn_{std::forward<fn_t>(fn)}
        , executor_(ex)
        {
        }
    [[nodiscard]]
    auto get_future(){
        return promise_.get_future();
    }

    template<UnaryFunction<result_t> function_type>
    auto then(const function_type &what){
        // Return type of what() call
        using r = typename std::invoke_result<function_type, result_t>::type;
        auto tsk = std::make_shared<task<r>>(executor_,
            [what, parent{shared_from_this()}] () mutable {
                auto future = static_cast<task<result_t>&>(*parent).get_future();
                return what(future.get());
            }
        );
        schedule_next(tsk);
        return tsk;
    }

    template<UnaryFunction<result> fn_t, UnaryFunction<result> ...more>
    auto then_fork(const fn_t &fn, more... fns){
        auto tasks_tuple = make_tasks_tuple(get_future().share(), fn, fns...);

        using r = decltype(tasks_helpers::wait_for_tasks(tasks_tuple));

        auto fork_join_task = std::make_shared<task<r>>(executor_,
             [tasks_tuple{std::move(tasks_tuple)}, parent{shared_from_this()}, exec{&this->executor_}]() mutable {
                 // Unfortunately, one thread in the pool will have to block until
                 // all continuation tasks are finished.
                 // We'll find a way to solve this when we talk about coroutines
                 std::stringstream s;
                 s << "Fork/join " << std::tuple_size<decltype(tasks_tuple)>() << " tasks";
                 set_task_name(s.str());

                 // Fork part: schedule the tasks in executor!
                 // Performance issue: there is a delay between this task being finished,
                 // and continuation tasks starting.
                 // We could solve this by adding a separate fork task,
                 // but that would block an entire thread
                 tasks_helpers::schedule_tasks(*exec, tasks_tuple);

                 return tasks_helpers::wait_for_tasks(tasks_tuple);
             }
         );
         schedule_next(fork_join_task);
         return fork_join_task;
    }
private:
    void schedule_next(executable_ptr tsk){
        {
            std::scoped_lock lock{mutex_};
            if (!has_finished_){
                next_ = tsk;
                return;
            }
        }
        // this has finished executing! Schedule the task directly with executor
        executor_.schedule(std::move(tsk));
    }

    template<typename shared_future_t, UnaryFunction<result> fn_t, UnaryFunction<result> ...more>
    auto make_tasks_tuple(shared_future_t sf, const fn_t &fn, more&... fns){
        return std::tuple_cat(make_tasks_tuple(sf, fn), make_tasks_tuple(sf, fns...));
    }

    //TODO: the following 2 functions are work in progess
    template<typename shared_future_t, UnaryFunction<result> fn_t>
    auto make_tasks_tuple(shared_future_t sf, const fn_t &fn){
        using r = typename std::invoke_result<fn_t, result>::type;
        auto tsk = std::make_shared<task<r>>(executor_,
            [future{std::move(sf)}, fn, parent{shared_from_this()}]() mutable {
                set_task_name("Fork wrapper");
                std::cout << "Waiting for parent task" << std::endl;
                return fn(future.get());
            }
        );
        return std::make_tuple(tsk);
    }
    void execute() noexcept override {
        try {
            execute_impl();
        }
        catch(...){
            promise_.set_exception(std::current_exception());
        }

        // Run the continuation task
        std::scoped_lock lock{mutex_};
        has_finished_ = true;
        if (next_)
            next_->execute();
    }

    void execute_impl(){
        if constexpr (std::is_same<result_t, void>::value){
            // The function returns void, no value to pass to the promise
            fn_();
            promise_.set_value();
        }
        else {
            promise_.set_value(fn_());
        }
    }

    std::function<result_t(void)> fn_;
    std::promise<result_t> promise_;

    executable_ptr next_;
    std::mutex mutex_;

    executor &executor_;
    bool has_finished_ = false;
};

template<typename result>
using task_ptr = std::shared_ptr<task<result>>;

template<NullaryFunction function_type>
inline auto run_task(executor &ex, function_type &&fn){
    auto t = std::make_shared<task<decltype(fn())>>(ex, std::forward<function_type>(fn));
    ex.schedule(t);
    return t;
}