#include <iostream>
#include <condition_variable>

template <class ConditionVariable = std::condition_variable>
class CyclicBarrier {
public:
    CyclicBarrier(size_t num_threads) {
        number_of_threads = num_threads;
        number_of_came_up = 0;
        number_of_passed = num_threads;
    }

    void Pass() {
        std::unique_lock<std::mutex> mutex(mutex0);
        while (number_of_passed < number_of_threads) {
            all_passed.wait(mutex);
        }

        ++number_of_came_up;
        while (number_of_came_up < number_of_threads) {
            all_came_up.wait(mutex);
        }
        all_came_up.notify_one();
        if (number_of_passed == number_of_threads) {
            number_of_passed = 0;
        }
        ++number_of_passed;
        if (number_of_passed == number_of_threads) {
            number_of_came_up = 0;
            all_passed.notify_all();
        }
        return;
    }

private:
    std::mutex mutex0;
    ConditionVariable all_came_up;
    ConditionVariable all_passed;
    size_t number_of_threads;
    size_t number_of_came_up;
    size_t number_of_passed;
};
