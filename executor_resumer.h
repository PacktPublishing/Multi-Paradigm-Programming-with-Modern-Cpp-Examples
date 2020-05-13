#pragma once
#include "executor.h"
#include <experimental/coroutine>

// Resumes a collection of coroutine handles at once.
// Each handle can be resumed on a different executor.
class executor_resumer{
    public:
    using handle_type = std::experimental::coroutine_handle<>;

    void add(handle_type h, executor *ex){
        while (lock.test_and_set(std::memory_order_acquire))
            ; // spin
        handles.push_back(std::make_pair(h, ex));
        lock.clear(std::memory_order_release);
    }

    void resume_all(){
        while (lock.test_and_set(std::memory_order_acquire))
            ; // spin
        for (auto p: handles)
            resume_on_executor(p.first, p.second);
        handles.clear();
        lock.clear(std::memory_order_release);
    }

    static void resume_on_executor(handle_type h, executor *ex){
        ex->schedule(std::make_shared<resumer>(h));
    }

    struct resumer : public executable {
        handle_type h;
        resumer(handle_type handle) : h{handle}
        {};

        void execute() noexcept override{
            h.resume();
        }
    };

    private:
    std::vector<std::pair<handle_type, executor*>> handles;
    std::atomic_flag lock;
};

