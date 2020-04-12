#pragma once

#include <thread>
#include <future>
#include <vector>
#include <queue>
#include <type_traits>
#include <memory>

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
    task(fn_t &&fn)
        : fn_{std::forward<fn_t>(fn)}{
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
        auto tsk = std::make_shared<task<r>>(
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
        auto tsk = std::make_shared<task<r>>(
            [what, parent{shared_from_this()}]() mutable {
                // Exception in parent future would get re-thrown
                static_pointer_cast<task<result_t>>(parent)->get_future().get();
                return what();
            }
        );
        then_ = tsk;
        return tsk;
    }

    //TODO: the following 2 functions are work in progess
    template<UnaryFunction<result> fn_t>
    auto fork(const function_type &fn){

    }

    template<UnaryFunction<result> fn_t, UnaryFunction<result> ...more>
    auto fork(const function_type &fn, more... fns){

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
    auto t = std::make_shared<task<decltype(fn())>>(std::forward<function_type>(fn));
    ex.schedule(t);
    return t;
}