#include "tasks.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#include <future>

using namespace std::chrono_literals;

int main(int argc, char *argv[]) {

    using namespace std;

    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};

    constexpr auto parallelism_level = 4;
    executor ex { parallelism_level };

    auto future = run_task(ex, [&daily_price](){
        auto average = 0.0;
        for (auto p: daily_price)
            average += p;
        average /= daily_price.size();
        return average;
    })->then(
        [&daily_price](double average){
            auto sum_squares = 0.0;
            for (auto price: daily_price){
                auto distance = price - average;
                sum_squares += distance * distance;
            }
            return sqrt(sum_squares / (daily_price.size() - 1));
        }
    )->then([](double stddev){
        std::cout << "Result: " << stddev << std::endl;
    })->get_future();

    std::cout << "Executing..." << std::endl;
    future.wait();

    cout << "Finished" << endl;
}