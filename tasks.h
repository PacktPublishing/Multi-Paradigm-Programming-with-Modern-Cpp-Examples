#pragma once

#include "executor.h"

#include <future>
#include <type_traits>
#include <memory>
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
    task(executor &e, fn_t &&fn)
        : executor_{e}
        , fn_{std::forward<fn_t>(fn)}{
    }

    // Can only be called once. Can't be called when using then()
    auto get_future() {
        return promise_.get_future();
    }

    // Continuation: Schedule a task immediately after this finishes
    // Then can be used to pass results between tasks
    // The beauty is that the tasks get executed bypassing the queue (on the same thread as previous task)
    template<UnaryFunction<result> function_type>
    auto then(const function_type &what) {
        using r = typename std::invoke_result<function_type, result_t>::type;
        auto tsk = std::make_shared<task<r>>(executor_,
            [what, parent{shared_from_this()}]() mutable {
                // Exception in parent future would get re-thrown
                executor::set_task_name("Continuation wrapper");
                return what(static_pointer_cast<task<result_t>>(parent)->get_future().get());
            }
        );
        then_ = tsk;
        return tsk;
    }

    // Same as above, but for void 
    template<NullaryFunction function_type>
    auto then(const function_type &what){
        using r = typename std::invoke_result<function_type>::type;
        auto tsk = std::make_shared<task<r>>(executor_,
            [what, parent{shared_from_this()}]() mutable {
                // Exception in parent future would get re-thrown
                static_pointer_cast<task<result_t>>(parent)->get_future().get();
                return what();
            }
        );
        then_ = tsk;
        return tsk;
    }

    template<UnaryFunction<result> fn_t, UnaryFunction<result> ...more>
    auto then_fork(const fn_t &fn, more... fns){
        auto tasks_tuple = make_tasks_tuple(get_future().share(), fn, fns...);

        using r = decltype(tasks_helpers::wait_for_tasks(tasks_tuple));

         auto join_task = std::make_shared<task<r>>(executor_,
             [tasks_tuple{std::move(tasks_tuple)}, parent{shared_from_this()}]() mutable {
                 // Unfortunately, one thread in the pool will have to block until
                 // all continuation tasks are finished.
                 // We'll find a way to solve this when we talk about coroutines
                 std::stringstream s;
                 s << "Join " << std::tuple_size<decltype(tasks_tuple)>() << " tasks";
                 executor::set_task_name(s.str());
                 return tasks_helpers::wait_for_tasks(tasks_tuple);
             }
         );
         then_ = join_task;
         return join_task;
    }


private:
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
                executor::set_task_name("Fork wrapper");
                std::cout << "Waiting for parent task" << std::endl;
                return fn(future.get());
            }
        );
        // Fork must schedule the task in executor!
        executor_.schedule(tsk);
        return std::make_tuple(tsk);
    }

    void execute() noexcept override {
        try {
            try {
                execute_impl();
            }
            catch(...){
                std::stringstream s;
                s << "Excepion thrown from task: ";
                
                if (auto name = executor_.get_task_name(); !name.empty())
                    s << name;
                else
                    s << "<unnamed task>";
                s << " (" << std::hex << this << ")";
                std::throw_with_nested(std::runtime_error(s.str()));
            }
        }
        catch(...){
            promise_.set_exception(std::current_exception());
        }
        executor_.set_task_name(std::string{});
        if (then_){
            then_->execute();
        }
    }

    void execute_impl() {
        promise_.set_value(fn_());
    }

    executor &executor_;
    std::function<result_t(void)> fn_;
    std::promise<result_t> promise_;

    executable_ptr then_;
};

// promise<void> does not have an argument in set_value
// Hence this specialization
template<>
inline void task<void>::execute_impl() {
   fn_();
   promise_.set_value();
}

// Like std::async, but requires executor and returns task
template<NullaryFunction function_type>
inline auto run_task(executor &ex, function_type &&fn){
    auto t = std::make_shared<task<decltype(fn())>>(ex, std::forward<function_type>(fn));
    ex.schedule(t);
    return t;
}

template<typename result>
using task_ptr = std::shared_ptr<task<result>>;