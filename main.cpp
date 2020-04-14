#include "tasks.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#include <future>

using namespace std::chrono_literals;

// prints the explanatory string of an exception. If the exception is nested,
// recurses to print the explanatory of the exception it holds
void print_exception(const std::exception& e, int level =  0)
{
    std::cerr << std::string(level, ' ') << "exception: " << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    } catch(const std::exception& e) {
        print_exception(e, level+1);
    } catch(...) {}
}


template<typename duration_t>
void loop_for(duration_t &&how_long){
    for (auto start = std::chrono::steady_clock::now(), now = start;
        now < start + how_long;
        now = std::chrono::steady_clock::now()) 
        {}  
}

template <typename iterator_t>
auto find_above_average(iterator_t from, iterator_t to, double average) {
    std::vector<double> above_average;

    std::copy_if(from, to,
                 std::back_inserter(above_average),
                 [average](double price) {
                     std::cout << "Performing a long operation..." << std::endl;
                     loop_for(10s);
                     return price > average;
                 });
    return above_average;
}

int main(int argc, char *argv[]) {

    using namespace std;

    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};

    executor exec{4};

    // Async will take care of thread creation
    auto future = run_task(exec,
        [&daily_price](){
            executor::set_task_name("Calculate average value");
            std::cout << "Calculation started..." << std::endl;
            auto average = 0.0;
            for (auto p: daily_price){
                loop_for(200ms);
                average += p;
            }
            average /= daily_price.size();
            return average;
        }
    )->then_fork(
        [&daily_price](double average){
            executor::set_task_name("Find standard deviation");
            auto sum_squares = 0.0;
            for (auto price: daily_price){
                auto distance = price - average;
                sum_squares += distance * distance;
            }
            return sqrt(sum_squares / (daily_price.size() - 1));
        },
        [&daily_price, &exec](double average){
            executor::set_task_name("Find items above average");
            
            vector<double> above_average;

            constexpr auto max_concurrency = 6;

            auto from = daily_price.begin();
            const auto step = 1 + daily_price.size() / max_concurrency;
            auto to = from + step;

            vector<task_ptr<vector<double>>> tasks;

            for (auto i = 0; i < max_concurrency; ++i){
                tasks.push_back(run_task(exec, [average, from, to](){
                    return find_above_average(from, to, average);
                }));

                from = to;
                to += step;
                if (to > daily_price.end()){
                    to = daily_price.end();
                }
            }
            

            // "Join"
            for (auto &t: tasks){
                auto task_result = t->get_future().get();
                above_average.insert(above_average.end(), task_result.begin(), task_result.end());
            }

            return above_average;
        }
    )->then(
        [&exec](auto &&result_tuple){
            executor::set_task_name("Final join");
            cout << "Standard deviation: " << std::get<0>(result_tuple) << std::endl;
            cout << "Elements above average: " << std::get<1>(result_tuple).size() << std::endl;
        }
    )->get_future();

    cout << "Calculating..." << endl;

    try {
        future.get();
    }
    catch(const std::exception &e){
        print_exception(e);
    }

    cout << "Finished" << endl;
}