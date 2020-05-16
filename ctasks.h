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
concept Task = requires (T t) {
    typename T::task_type;
    typename T::promise_type;
    typename T::handle_type;
};

// Debug output for a task
inline void ctask_debug(std::string msg){
    static std::mutex print_mutex;
    std::scoped_lock lock{print_mutex};
    std::cerr << "\t" << msg << std::endl;
}

using tasks_executor_provider = executor_provider<>;

template<Task task_t>
class schedule_task : public std::experimental::suspend_always{
    public:
    void await_suspend(typename task_t::handle_type handle) const noexcept {
        auto &promise = handle.promise();
        tasks_executor_provider::executor().schedule(promise.get_state());
    }
};

template<Task task_t>
class ctask_awaiter {
    public:
    ctask_awaiter(task_t t) : task_{std::move(t)}
    {}

    bool await_ready() const noexcept {
        return task_.ready();
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle) noexcept {
        if (task_.ready())
            return false;
        task_.add_continuation(handle);
        return true;
    }

    auto await_resume() const {
        // e. g. 
        // task<int> tsk = calculate();
        // int x = co_await tsk;
        return task_.get();
    }

    private:
    task_t task_;
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
    ctask()
        : shared_state_ { std::make_shared<state>() }
        , shared_future_{ shared_state_->result.get_future() }
    {}

    auto get() const {
        return shared_future_.get();
    }

    auto wait() const {
        shared_future_.wait();
    }

    bool ready() const {
        return shared_future_.wait_for(std::chrono::duration<size_t>::zero()) == std::future_status::ready;
    }

    void add_continuation(std::experimental::coroutine_handle<> handle){
        shared_state_->continuations.add(handle, &tasks_executor_provider::executor());
        if (ready()){
            shared_state_->continuations.resume_all();
        }
    }

    using promise_type = coroutine_promise;
    using task_type = ctask<result_t>;
    using handle_type = std::experimental::coroutine_handle<task_type::promise_type>;

    private:
    std::shared_ptr<state> shared_state_;
    std::shared_future<result_t> shared_future_;
};

// Shared between all instances of a task, and keeps the coroutine handle.
template<TaskResult result_t>
struct ctask<result_t>::state : public executable {
    void execute() noexcept override {
        ctask_debug(" [Resuming on executor thread] ");
        handle.resume();
    }

    std::promise<result_t> result;
    handle_type handle;
    executor_resumer continuations;
};

// Coroutine promise for the ctask
template<TaskResult result_t>
struct ctask<result_t>::coroutine_promise {

    void debug(std::string msg){
        ctask_debug(name_ + " [" + msg + "]");
    }

    auto get_return_object() {
        auto tsk = task_type{};
        shared_state_ = tsk.shared_state_;
        tsk.shared_state_->handle = handle_type::from_promise(*this);
        return tsk;
    }

    auto initial_suspend() {
        debug("Initial suspend");
        return schedule_task<task_type>{};
    }

    auto final_suspend() {
        debug("Final suspend");
        return std::experimental::suspend_never{};
    }

    template<TaskResult T>
    auto return_value(T &&value){
        debug ("Return value");
        auto state = get_state();
        state->result.set_value(std::forward<T>(value));
        state->continuations.resume_all();
    }

    auto unhandled_exception() {
        get_state()->result.set_exception(std::current_exception());
    }

    template<Task other_task_t>
    auto await_transform(other_task_t other_task){
        debug("Wait for another task to complete");
        return ctask_awaiter<other_task_t>{std::move(other_task)};
    }

    auto await_transform(std::string name){
        name_ = name;
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

