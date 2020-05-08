#include "tasks.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#include <future>

#include <experimental/coroutine>

using namespace std::chrono_literals;

class coroutine_result {
    public:
    int value = 0;

    struct promise_type;
    using handle = std::experimental::coroutine_handle<promise_type>;

    struct promise_type{

        coroutine_result get_return_object() {
            std::cout << " promise_type::get_return_object()" << std::endl;
            return coroutine_result{1};
        }

        auto initial_suspend() { 
            std::cout << " promise_type::initial_suspend()" << std::endl;
            return std::experimental::suspend_never{};
        }

        auto final_suspend() { 
            std::cout << " promise_type::final_suspend()" << std::endl;
            return std::experimental::suspend_never{}; 
        }

        void unhandled_exception() { 
            std::cout << " promise_type::unhandled_exception()" << std::endl;
            std::terminate();
        }

        void return_void(){
            std::cout << " promise_type::return_void()" << std::endl;
        }
    };
};

/*
class awaiter : public std::experimental::suspend_always {
    public:
    bool await_ready() const noexcept {
         std::cout << " awaitable::await_ready()" << std::endl;
         return false;
    }
    void await_suspend(std::experimental::coroutine_handle<>) const noexcept {
         std::cout << " awaitable::await_suspend()" << std::endl;
    }
    void await_resume() const noexcept {
         std::cout << " awaitable::await_resume()" << std::endl;
    }
};
*/

coroutine_result my_coro(){
    std::cout << "In coroutine" << std::endl;
    std::cout << "Suspending coroutine..." << std::endl;
    co_await std::experimental::suspend_always{};
    std::cout << "Coroutine resumed!" << std::endl;
}

int main(int argc, char *argv[]) {

    using namespace std;

    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};

    constexpr auto parallelism_level = 8;
    executor ex { parallelism_level };

    std::cout << "Executing..." << std::endl;

    auto result = my_coro();
    std::cout << "Corutine has returned " << result.value << endl;
}