#include "ctasks.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

void print_thread_id(){
    std::cout << "Current thread ID: " << std::this_thread::get_id() << std::endl;
}

struct alternative_exp{
    static executor &executor() {
        static class executor ex{8};
        return ex;
    }
};

ctask<int> multiply(int a, int b){
    // We are in an executor thread now!
    print_thread_id();
    co_return a * b;
}

ctask<int> mul_add(int a, int b, int c, int d){

    auto tsk = multiply(a, b);
    auto p2 = co_await multiply(c, d);
    auto p1 = co_await tsk;

    co_return p1 + p2;
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

    using namespace std;
    constexpr size_t parallelism_level = 4;
    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};

    std::cout << "Compute average" << std::endl;

    // Compute average (resume once available)
    auto average = co_await [&daily_price]() -> ctask<double> {
        co_await "Average";

        auto average = 0.0;
        for (auto p: daily_price)
            average += p;
        average /= daily_price.size();
        co_return average;
    }();

    std::cout << "Compute standard deviation" << std::endl;

    // Fork branch 1: compute stddev (no co_await: do not suspend!)
    auto stddev_task = [&daily_price](double average) -> ctask<double>{
            co_await "StdDev";

            auto sum_squares = 0.0;
            for (auto price: daily_price){
                auto distance = price - average;
                sum_squares += distance * distance;
            }
            co_return sqrt(sum_squares / (daily_price.size() - 1));
        }(average);
    
    std::cout << "Find items above average" << std::endl;

    // For branch 2: find items above average (no co_await)
    auto above_average_task =
        [&daily_price](double average) -> ctask<vector<double>>{
            co_await "AboveAverage";

            vector<double> above_average;
            const auto nof_chunks = parallelism_level;
            const auto chunk_size = daily_price.size() / nof_chunks;

            auto from = daily_price.begin();
            auto to = from + chunk_size;

            auto start = chrono::steady_clock::now();

            std::cout << nof_chunks << " tasks must be created" << std::endl;

            vector<ctask<vector<double>>> tasks;
            for (auto i = 0; i < nof_chunks; ++i){
                if (i == nof_chunks - 1)
                    to = daily_price.end();
                
                tasks.push_back(
                    find_above_average(from, to, average)
                );

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
    std::cout << "In main" << std::endl;
    print_thread_id();


    //auto tsk = mul_add(17, 24, 21, 2);
    //std::cout << tsk.get() << std::endl;

    // Do not let main() return until fork_join_example finishes
    return fork_join_example().get();
}
