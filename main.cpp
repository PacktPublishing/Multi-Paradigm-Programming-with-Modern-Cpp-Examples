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
    auto stddev_task = run_task(exec,
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
    )->then([&daily_price](double average){
        auto sum_squares = 0.0;
        for (auto price: daily_price){
            auto distance = price - average;
            sum_squares += distance * distance;
        }
        return sqrt(sum_squares / (daily_price.size() - 1));
    })->then([](double stddev){
        cout << stddev << endl;
    })->then([](){
        cout << "Done";
    });

    cout << "Calculating..." << endl;
    //cout << "Standard deviation: " << stddev_task->get_future().get() << endl;


}