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

    // Async will take care of thread creation
    auto future_average = std::async(std::launch::deferred,
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
    );
    std::this_thread::sleep_for(200ms);

    // Wait is optional
    //while (future_average.wait_for(100ms) != future_status::ready){
    //    cout << "."; // "progress bar"
    //    cout.flush();
    //}
    cout << "Average value: " << flush << future_average.get() << endl;

}