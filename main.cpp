#include "tasks.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#include <future>

int main(int argc, char *argv[]) {

    using namespace std;
    using namespace std::chrono_literals;

    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};

    executor exec{24};

    // Async will take care of thread creation
    auto future = run_task(exec,
        [&daily_price](){
            std::cout << "Calculation started..." << std::endl;
            auto average = 0.0;
            for (auto p: daily_price){
                std::this_thread::sleep_for(200ms);
                average += p;
            }
            average /= daily_price.size();
            return average;
        }
    )->then_fork(
        [&daily_price](double average){
            auto sum_squares = 0.0;
            for (auto price: daily_price){
                auto distance = price - average;
                sum_squares += distance * distance;
            }
            return sqrt(sum_squares / (daily_price.size() - 1));
        },
        [&daily_price](double average){
            throw std::runtime_error("oops");
            std::vector<double> above_average;
            std::copy_if(daily_price.begin(), daily_price.end(),
                std::back_inserter(above_average),
                [average](double price){ return price > average; });
            return above_average;
        }
    )->then([](auto &&result_tuple){
        cout << "Standard deviation: " << std::get<0>(result_tuple) << std::endl;
        cout << "Elements above average: " << std::get<1>(result_tuple).size() << std::endl;
    })->get_future();


    try {
        future.get();
    }
    catch(const std::exception &e){
        cout << "A task has thrown an exception: " << e.what() << std::endl;
    }
    cout << "Calculating..." << endl;
    //cout << "Standard deviation: " << stddev_task->get_future().get() << endl;

    std::this_thread::sleep_for(20s);

}