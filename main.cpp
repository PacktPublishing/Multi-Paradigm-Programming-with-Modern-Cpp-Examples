#include <iostream>
#include <memory>
#include <experimental/coroutine>

class lazy_int {
    public:
    int value() const {
        if (auto handle = shared_state_->handle){
            // Resume the coroutine to calculate the final value
            handle.resume();
        }
        // When we get here, the coroutine has resumed and returned 
        return shared_state_->value;
    }

    struct promise_type;
    using handle_type = std::experimental::coroutine_handle<promise_type>;

    private:
    lazy_int(handle_type handle)
        : shared_state_{std::make_shared<state>()}
    {
        shared_state_->handle = handle;
    }

    struct state{
        int value = 0;
        handle_type handle;

        ~state(){
            if (handle)
                handle.destroy();
        }
    };

    std::shared_ptr<state> shared_state_;
};

struct lazy_int::promise_type {
    std::weak_ptr<state> shared_state_;

    auto get_return_object() {
        std::cout<< "promise_type::get_return_object()" << std::endl;

        auto handle = handle_type::from_promise(*this);

        auto return_object = lazy_int{handle};
        shared_state_ = return_object.shared_state_;
        return return_object;
    }

    auto initial_suspend() {
        std::cout << "promise_type::initial_suspend()" << std::endl;
        return std::experimental::suspend_always{};
    }

    auto final_suspend() {
        std::cout << "promise_type::final_suspend()" << std::endl;
        // Avoid double-deletion of the coroutine frame
        if (auto state = shared_state_.lock())
            state->handle = nullptr;

        return std::experimental::suspend_never{};
    }

    auto return_value(int value){
        std::cout << "promise_type::return_value()" << std::endl;
        if (auto state = shared_state_.lock())
            state->value = value;
    }

    auto unhandled_exception() {
        std::cout << "promise_type::unhandled_exception()" << std::endl;
        std::terminate();
    }

};

lazy_int my_coro(){
    std::cout << "The coroutine has resumed" << std::endl;

    co_return 42;
}

lazy_int another_coro(){
    int value = 0;
    // Only ask for the value when the caller needs to use it
    std::cout << "Please enter a value: ";
    std::cin >> value;
    co_return value;
}

int main(int argc, char *argv[]) {
    std::cout << "Calling the coroutine..." << std::endl;
    auto result = my_coro();
    //std::cout << "Coroutine has returned " << result.value() << std::endl;
}
