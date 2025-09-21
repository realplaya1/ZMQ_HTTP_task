#ifndef THREAD_QUEUE_H
#define THREAD_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;

template <typename T>
class ThreadQueue {
public:
    void push(T value) {
        lock_guard<mutex> lock(mtx_);
        queue_.push(move(value));
        cv_.notify_one();
    }

    // Блокируется, пока не появится элемент
    bool wait_and_pop(T& value) {
        unique_lock<mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty() || stop_requested_; });
        if (stop_requested_ && queue_.empty()) {
            return false; // Завершение работы
        }
        value = move(queue_.front());
        queue_.pop();
        return true;
    }

    void stop() {
        lock_guard<mutex> lock(mtx_);
        stop_requested_ = true;
        cv_.notify_all();
    }

private:
    queue<T> queue_;
    mutex mtx_;
    condition_variable cv_;
    bool stop_requested_ = false;
};

#endif // THREAD_QUEUE_H