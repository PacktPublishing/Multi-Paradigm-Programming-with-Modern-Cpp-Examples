#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

int main(int argc, char *argv[]) {

    using namespace std;
    using namespace std::chrono_literals;

    vector<double> daily_price = { 100.3, 101.5, 99.2, 105.1, 101.93,
                                   96.7, 97.6, 103.9, 105.8, 101.2};

    double average = 0.0;
    std::atomic_flag still_busy = true; // atomic<bool>

    // Calculate average in a separate thread
    std::thread thread {
        [&daily_price, &average, &still_busy](){
            for (auto p: daily_price){
                std::this_thread::sleep_for(2ms);
                average += p;
            }
            average /= daily_price.size();
            still_busy.clear(std::memory_order_release);

            std::this_thread::sleep_for(2s);
        }
    };

    while (still_busy.test_and_set(std::memory_order_acquire)){
        cout << "..."; // "progress bar"
        std::this_thread::sleep_for(1ms);
    }
    cout << "Average value: " << average << endl;

    thread.join(); // wait until the thread finishes
    cout << "Worker has finished working" << endl;
}