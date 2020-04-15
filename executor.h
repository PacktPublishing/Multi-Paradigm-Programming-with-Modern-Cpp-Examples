#pragma once

#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

// Base class for all kinds of tasks that can be executed
class executable : public std::enable_shared_from_this<executable>{
    public:
    virtual void execute() noexcept = 0;
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

    static void set_task_name(std::string name) noexcept{
        task_name_ = std::move(name);
    }

    static const std::string &get_task_name() noexcept {
        return task_name_;
    }

    private:

    void add_thread() {
        threads_.emplace_back(
            [this]() {
                run_executor_thread();
            });
    }

    void run_executor_thread() noexcept {
        while (active_) {
            // Wait for a new executable...
            std::unique_lock cv_lock{wakeup_mutex_};
            wakeup_.wait(cv_lock);
            // ...don't make other executor threads wait while this one gets executed
            cv_lock.unlock();

            execute_next();
        }
    }

    void execute_next() noexcept
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
        task_name_.clear();
    }

    private:
    static thread_local std::string task_name_;

    std::vector<std::thread> threads_;
    std::queue<executable_ptr> queue_;
    std::mutex queue_mutex_;

    std::mutex wakeup_mutex_;
    std::condition_variable wakeup_;

    std::atomic<bool> active_ = true;
};
