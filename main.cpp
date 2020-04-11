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

    std::promise<double> average_promise;

    // Calculate average in a separate thread
    std::thread thread {
        [&daily_price, &average_promise](){
            auto average = 0.0;

            for (auto p: daily_price){
                std::this_thread::sleep_for(200ms);
                average += p;
            }
            average /= daily_price.size();
            average_promise.set_value(average);

            std::this_thread::sleep_for(2s);
        }
    };

    auto future_average = average_promise.get_future();

    while (future_average.wait_for(100ms) != future_status::ready){
        cout << "."; // "progress bar"
        cout.flush();
    }
    cout << "Average value: " << future_average.get() << endl;

    thread.join(); // wait until the thread finishes
    cout << "Worker has finished working" << endl;
}