#include "ctasks.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace std::chrono_literals; using namespace std;

void print_thread_id(){
    std::cout << "Current thread ID: " << std::this_thread::get_id() << std::endl;
}

ctask<int> test_coro(){
    print_thread_id();
    co_return 42;
}

ctask<int> multiply(int a, int b){
    co_await "Multiply";
    co_return a * b;
}

ctask<int> mul_add(int a, int b, int c, int d){
    co_await "Mul_Add";
    auto p1 = multiply(a, b);
    co_return co_await multiply(c, d) + co_await p1;
}

template<typename iterator_t>
ctask<std::vector<double>> find_above_average(iterator_t from, iterator_t to, double average){
    co_await "ChunkAboveAverage";
    using namespace std::chrono_literals;
    std::vector<double> above_average;
    std::copy_if(from, to,
        std::back_inserter(above_average),
            [average](auto price){
                std::this_thread::sleep_for(2s);
                return price > average;
            });

    co_return above_average;
}

ctask<int> fork_join_example(){
    co_await "Root";
    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};
    auto average = co_await [&daily_price]() -> ctask<double> {
        co_await "Average";
        auto average = 0.0;
        for (auto p: daily_price)
            average += p;
        average /= daily_price.size();
        co_return average;
    }();

    auto stddev_task = [&daily_price](double average) -> ctask<double>{
        co_await "StdDev";
        auto sum_squares = 0.0;
        for (auto price: daily_price){
            auto distance = price - average;
            sum_squares += distance * distance;
        }
        co_return sqrt(sum_squares / (daily_price.size() - 1));
    } (average);

    auto above_average_task = [&daily_price](double average) -> ctask<vector<double>> {
        co_await "AboveAverage";
        vector<double> above_average;
        const auto nof_chunks = 4; // parallelism level
        const auto chunk_size = daily_price.size() / nof_chunks;

        auto start = chrono::steady_clock::now();

        auto from = daily_price.begin();
        auto to = from + chunk_size;
        vector<ctask<vector<double>>> tasks;
        for (auto i = 0; i < nof_chunks; ++i){
            if (i == nof_chunks - 1)
                to = daily_price.end();
            tasks.push_back(find_above_average(from, to, average));
            from = to;
            to += chunk_size;
        }

        for (auto &t: tasks){
            auto task_result = co_await t;
            above_average.insert(above_average.end(), task_result.begin(), task_result.end());
        }

        cout << "Elapsed time in seconds: "
                 << chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - start).count()
                 << endl;

        co_return above_average;
    }(average);

    cout << "Standard deviation: " << co_await stddev_task << endl;
    cout << "Elements above average: " << (co_await above_average_task).size() << endl;
    co_return 0;
}

int main(int argc, char *argv[]) {
    return fork_join_example().get();
}
