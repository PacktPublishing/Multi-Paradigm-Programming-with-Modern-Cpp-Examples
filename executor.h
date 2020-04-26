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
            threads_.emplace_back(&executor::run_thread, this);
        }
    }

    ~executor() {
        // Notify all threads that we're shutting down
        active_.store(false, std::memory_order_release);
        wakeup_.notify_all();

        // All threads need to be joined before they're destroyed
        for (auto &t: threads_){
            t.join();
        }
    }

    void schedule(executable_ptr what){
        if (!active_.load(std::memory_order_acquire)){
            throw std::runtime_error("Executor is being destroyed. You can't schedule any more work.");
        }

        {
            std::scoped_lock lock(mutex_);
            queue_.push(std::move(what));
        }
        // One thread can pick up the task
        wakeup_.notify_one();
    }

    private:

    void run_thread() noexcept {
        while (true)
        {
            std::unique_lock lock{mutex_};

            // Wait, and ignore notifications while the queue is empty and executor is active
            wakeup_.wait(lock, [this]() {
                auto active = this->active_.load(std::memory_order_acquire);
                return !(this->queue_.empty() && active);
            });

            if (queue_.empty())
                break;

            // Queue is not empty. Pop an element and execute
            auto next = std::move(queue_.front());
            queue_.pop();
            // Don't make anyone wait until this task executes
            lock.unlock();

            next->execute();
        }
    }

    private:

    std::vector<std::thread> threads_;

    std::mutex mutex_;
    std::queue<executable_ptr> queue_;
    std::condition_variable wakeup_;

    std::atomic<bool> active_ = true;
};
