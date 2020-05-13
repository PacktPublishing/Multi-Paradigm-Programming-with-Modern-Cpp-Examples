#pragma once
#include "executor.h"
#include "executor_resumer.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <experimental/coroutine>
#include <future>
#include <vector>
#include <mutex>

// Result of a task
template<typename T>
concept TaskResult = 
    std::is_copy_constructible<T>::value ||
    std::is_move_constructible<T>::value ||
//    std::is_same<T, void>::value ||
    std::is_reference<T>::value;

// A coroutine-based task
template<typename T>
concept Task = requires {
    typename T::task_type;
    typename T::promise_type;
};

// Debug output for a task
inline void ctask_debug(std::string msg){
    static std::mutex print_mutex;
    std::scoped_lock lock{print_mutex};
    std::cerr << "\t" << msg << std::endl;
}

using tasks_executor_provider = executor_provider<>;

// Schedules the ctask to be resumed on an executor thread
template<Task task_t>
class schedule_task : public std::experimental::suspend_always{
    public:
    void await_suspend(typename task_t::handle_type handle) const noexcept {
        auto &promise = handle.promise();
        tasks_executor_provider::executor().schedule(promise.get_state());
    }
};

// Suspends the current task until another task finishes.
// Tasks don't have to be on the same executor.
// Resumption will always happen on the executor of the current task.
template<Task task_type>
class ctask_awaiter{
    public:
    ctask_awaiter(task_type t)
        : task_{std::move(t)}
    {
    }

    bool await_ready() const noexcept { 
        // Do not suspend if the task has already finished
        return task_.ready(); 
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle) noexcept {
        // The coroutine has been suspended

        // We want to resume once the task is ready
        // If it's become ready while we were suspending,
        // then resume immediately
        if (task_.ready())
            return false;

        // Tell the task to schedule a continuation
        task_.add_continuation(handle, tasks_executor_provider::executor());
        return true;
    }

    auto await_resume() const noexcept {
        return task_.get();
    }

    private:
    task_type task_;
};


// An asyncrhonous tasks that uses coroutines.
// A coroutine must return a ctask<T>. 
// The coroutine will always run on an executor thread.
// Executor to use can be specified through a template argument.
template<TaskResult result_t>
class ctask {
    struct coroutine_promise;
    struct state;

    public:
    // Picked up by coroutine_traits
    using promise_type = coroutine_promise;
    // A few shortcuts
    using task_type = ctask<result_t, exp>;
    using handle_type = std::experimental::coroutine_handle<task_type::promise_type>;
    using executor_provider = exp;

    // Initializes the task and its shared state.
    // Immediately grabs the shared future from the promise.
    // So all copies will share the state and the shared future.
    ctask()
        : shared_state_{std::make_shared<state>()}
        , shared_future_{shared_state_->result.get_future()}
    {
    }

    // Get task result. Blocks until the task finishes.
    auto get() const{
        return shared_future_.get();
    }

    // Blocks until the task finishes
    auto wait() const{
        shared_future_.wait();
    }

    // Returns true when the result is available
    auto ready() const{
        return shared_future_.wait_for(std::chrono::duration<size_t>::zero()) == std::future_status::ready;
    }

    // Add a coroutine handle to be resumed on an executor thread once this task finishes.
    // If the task has already finished, then the coroutine is resumed immediately (on an executor thread)
    void add_continuation(std::experimental::coroutine_handle<> handle, executor &continuation_ex){
        shared_state_->continuations.add(handle, &continuation_ex);
        // An additional guard in case the task has finished during the call to continuations.add
        if (ready()){
            shared_state_->continuations.resume_all();
        }
    }

    private:
    std::shared_ptr<state> shared_state_;
    std::shared_future<result_t> shared_future_;
};

//
// Shared between all instances of a task, and keeps the coroutine handle.
// Implements executable interface.
// When passed to an executor, will resume() the coroutine when executed.
//
// Also launches continuations once finished
//
template<TaskResult result_t>
struct ctask<result_t, exp>::state : public executable {

    void execute() noexcept override{
        if (handle){
            ctask_debug(" [Resuming coroutine on an executor thread] ");
            handle.resume();
            continuations.resume_all();
        }
    }

    executor_resumer continuations;
    std::promise<result_t> result;
    handle_type handle;
};

// Coroutine promise for the ctask
template<TaskResult result_t>
struct ctask<result_t, exp>::coroutine_promise {

    void debug(std::string msg){
        ctask_debug(name_ + " [" + msg + "]");
    }

    auto get_return_object() {
        // Construct a task and store a pointer to its shared state
        auto tsk = task_type{};
        tsk.shared_state_->handle = handle_type::from_promise(*this);
        shared_state_ = tsk.shared_state_;
        return tsk;
    }

    auto initial_suspend() {
        debug("Initial suspend");
        return schedule_task<task_type>{};
    }

    auto final_suspend() {
        debug("Final suspend");
        if (auto state = shared_state_.lock())
            state->handle = nullptr;
        return std::experimental::suspend_never{};
    }

    template<TaskResult T>
    auto return_value(T &&value){
        debug ("Return value");
        get_state()->result.set_value(std::forward<T>(value));
        //TODO: whatabout void?
    }

    auto unhandled_exception() {
        get_state()->result.set_exception(std::current_exception());
    }

    template<Task other_task_t>
    auto await_transform(other_task_t other_task){
        debug("Wait for another task to complete");
        return ctask_awaiter<other_task_t>{std::move(other_task)};
    }

    // Hack: use "co_await <string>" to set the name of the promise
    auto await_transform(std::string name){
        name_ = name;
        debug("");
        return std::experimental::suspend_never{};
    }

    auto get_state(){
        auto state = shared_state_.lock();
        assert(state);
        return state;
    }

    private:
    std::weak_ptr<state> shared_state_;
    std::string name_;
};

