#include <iostream>
#include <deque>
#include <mutex>
#include <condition_variable>

template <class T, class Container = std::deque<T>>
class BlockingQueue {
public:
    explicit BlockingQueue(const size_t& capacity) {
        capacity_of_queue = capacity;
        number_of_items = 0;
        shutdown = false;
    }

    void Put(T&& element) {
        std::unique_lock<std::mutex> mutex(mutex0);
        while (!shutdown && number_of_items == capacity_of_queue) {
            full_queue.wait(mutex);
        }
        if (shutdown) {
            throw std::runtime_error("");
        } else {
            queue.push_back(std::move(element));
            new_item_in_queue.notify_all();
            ++number_of_items;
        }
        return;
    }

    bool Get(T& result) {
        std::unique_lock<std::mutex> mutex(mutex0);
        while (!shutdown && number_of_items == 0) {
            new_item_in_queue.wait(mutex);
        }
        if (!shutdown || number_of_items > 0) {
            result = std::move(queue.front());
            queue.pop_front();
            if (number_of_items == capacity_of_queue) {
                full_queue.notify_all();
            }
            --number_of_items;
            return true;
        } else {
            return false;
        }
    }

    void Shutdown() {
        std::unique_lock<std::mutex> mutex(mutex0);
        shutdown = true;
        full_queue.notify_all();
        new_item_in_queue.notify_all();
    }

private:
    size_t capacity_of_queue;
    size_t number_of_items;
    bool shutdown;
    Container queue;
    std::mutex mutex0;
    std::condition_variable new_item_in_queue;
    std::condition_variable full_queue;
};