#include "tasks.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#include <future>

using namespace std::chrono_literals;

template<typename iterator_t>
auto find_above_average(iterator_t from, iterator_t to, double average){
    std::vector<double> above_average;
    std::copy_if(from, to,
        std::back_inserter(above_average),
            [average](auto price){
                std::this_thread::sleep_for(2s);
                return price > average;
            });

    return above_average;
}
int main(int argc, char *argv[]) {

    using namespace std;

    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};

    constexpr auto parallelism_level = 8;
    executor ex { parallelism_level };

    auto future = run_task(ex, [&daily_price](){
        auto average = 0.0;
        for (auto p: daily_price)
            average += p;
        average /= daily_price.size();
        return average;
    })->then_fork(
        [&daily_price](double average){
            auto sum_squares = 0.0;
            for (auto price: daily_price){
                auto distance = price - average;
                sum_squares += distance * distance;
            }
            return sqrt(sum_squares / (daily_price.size() - 1));
        },
        [&daily_price, &ex](double average){
            std::vector<double> above_average;
            const auto nof_chunks = parallelism_level;
            const auto chunk_size = daily_price.size() / nof_chunks;

            auto from = daily_price.begin();
            auto to = from + chunk_size;

            auto start = chrono::steady_clock::now();

            vector<task_ptr<vector<double>>> tasks;
            for (auto i = 0; i < nof_chunks; ++i){
                if (i == nof_chunks - 1)
                    to = daily_price.end();
                
                tasks.push_back(
                    run_task(ex, [average, from, to](){
                        return find_above_average(from, to, average);
                    })
                );

                from = to;
                to += chunk_size;
            }

            for (auto &t: tasks){
                auto task_result = t->get_future().get();
                above_average.insert(above_average.end(), task_result.begin(), task_result.end());
            }

            cout << "Elapsed time in seconds: "
                 << chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - start).count()
                 << std::endl;
            return above_average;
        }
    )->then([](auto &&results_tuple){
        cout << "Standard deviation: " << std::get<0>(results_tuple) << std::endl;
        cout << "Elements above average: " << std::get<1>(results_tuple).size() << std::endl;
    })->get_future();

    std::cout << "Executing..." << std::endl;
    future.wait();

    cout << "Finished" << endl;
}