#pragma once

#include <thread>
#include <future>
#include <vector>
#include <queue>
#include <type_traits>
#include <memory>
#include <iostream>

// Base class for all kinds of tasks that can be executed
class executable : public std::enable_shared_from_this<executable>{
    public:
    virtual void execute() = 0;
};

using executable_ptr = std::shared_ptr<executable>;

// Runs executables in background threads
class executor {
    public:

    executor(size_t nof_threads) {
        // Launch a pool of threads
        for (size_t i = 0; i < nof_threads; ++i){
            add_thread();
        }
    }

    ~executor() {
        // All threads need to be joined before they're destroyed
        active_ = false;
        wakeup_.notify_all();
        for (auto &t: threads_){
            t.join();
        }
    }

    void schedule(executable_ptr what){
        {
            std::scoped_lock lock(queue_mutex_);
            queue_.push(std::move(what));
        }
        // One thread can pick up the task
        wakeup_.notify_one();
    }

    private:

    void add_thread() {
        threads_.emplace_back(
            [this]() {
                try {
                    run_executor_thread();
                }
                catch (...) {
                    // Indicates a bug in this code
                    // (exceptions in executables are caught and passed to the promise)
                    std::terminate();
                }
            });
    }

    void run_executor_thread() {
        while (active_) {
            // Wait for a new executable...
            std::unique_lock cv_lock{wakeup_mutex_};
            wakeup_.wait(cv_lock);
            // ...don't make other executor threads wait while this one gets executed
            cv_lock.unlock();

            execute_next();
        }
    }

    void execute_next()
    {
        std::unique_lock queue_lock{queue_mutex_};
        // Another thread might've popped this one in between
        if (queue_.empty())
            return;
        
        auto next = std::move(queue_.front());
        queue_.pop();
        // Don't make queue users wait while this one executes
        queue_lock.unlock();

        std::cout << "Executor Executing" << std::endl;
        next->execute();
    }

    private:
    std::vector<std::thread> threads_;
    std::queue<executable_ptr> queue_;
    std::mutex queue_mutex_;

    std::mutex wakeup_mutex_;
    std::condition_variable wakeup_;

    std::atomic<bool> active_ = true;
};

    template<typename t>
    inline auto join_all_(t &task){
        return std::make_tuple(task->get_future().get());
    }

    template<typename t, typename... tasks_t>
    inline auto join_all_(t &task, tasks_t& ...tasks){
        return std::tuple_cat(join_all_(task), join_all_(tasks...));
    }

    template<typename... tasks_t>
    inline auto join_all_x(tasks_t&& ...tasks){
        return join_all_(tasks...);
    }

    template<typename ...tasks_t>
    inline auto join_all(std::tuple<tasks_t...> &tasks){
        return std::apply([](auto &&... args){
            //return std::make_tuple(0, 1, 2);
            return join_all_x(args...);
        }, tasks);
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
        auto tasks_tuple = then_fork_(get_future().share(), fn, fns...);

        using r = decltype(join_all(tasks_tuple));

         auto join_task = std::make_shared<task<r>>(executor_,
             [tasks_tuple, parent{shared_from_this()}]() mutable {
                 // Unfortunately, one thread in the pool will have to block until
                 // all continuation tasks are finished.
                 // We'll find a way to solve this when we talk about coroutines
                 return join_all(tasks_tuple);
             }
         );
         then_ = join_task;
         return join_task;
    }


    template<typename shared_future_t, UnaryFunction<result> fn_t, UnaryFunction<result> ...more>
    auto then_fork_(shared_future_t sf, const fn_t &fn, more&... fns){
        return std::tuple_cat(then_fork_(sf, fn), then_fork_(sf, fns...));
    }

    //TODO: the following 2 functions are work in progess
    template<typename shared_future_t, UnaryFunction<result> fn_t>
    auto then_fork_(shared_future_t sf, const fn_t &fn){
        using r = typename std::invoke_result<fn_t, result>::type;
        auto tsk = std::make_shared<task<r>>(executor_,
            [future{std::move(sf)}, fn, parent{shared_from_this()}]() mutable {
                return fn(future.get());
            }
        );
        // Fork must schedule the task in executor!
        executor_.schedule(tsk);
        return std::make_tuple(tsk);
    }


private:
    void execute() override{
        try {
            execute_impl();
        }
        catch(...){
            promise_.set_exception(std::current_exception());
        }

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