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


struct return_type{
    // Your return type
};

struct promise_type {
    auto get_return_object() {                      // Return value of a coroutine
        return return_type{};
    }

    auto initial_suspend() {                        // Suspend a coroutine before running it?
        return std::experimental::suspend_never{};
    }

    auto final_suspend() {                          // Suspend a coroutine before exiting it?
        return std::experimental::suspend_never{};
    }

    void return_value(auto value){                  // Invoked when co_return is used.
                                                    // Update the return object here
    }

    auto yield_value(auto value){                   // Invoked when co_yield is used.
        return std::experimental::suspend_always{}; // Update the return object here
    }

    auto unhandled_exception() {                    // Invoked when coroutine code throws
        std::terminate();
    }
};

task<int> calculate_average(){
    co_await switch_to_task_thread{};
    double average = 0;
    // (calculaion code skipped)
    co_return average;
}


lazy_int my_coro(){
    std::cout << "The coroutine has resumed" << std::endl;

    co_return 42;
}

// This awaiter is from STL
// It suspends the coroutine unconditionally

struct suspend_always {

  bool await_ready() const noexcept {
      // Is the coroutine ready to resume immediately?
      // Returning true would make this suspend_never
      return false;
  }

  void await_suspend(coroutine_handle<>) const noexcept {
      // Called when the coroutine is suspended.
      // You can schedule resumption here.
  }

  void await_resume() const noexcept {
      // Called before the coroutine resumes.
      // You can do 
  }
};


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
