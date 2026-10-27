#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace concurx {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity), closed_(false) {
        if (capacity_ == 0) {
            throw std::invalid_argument("queue capacity must be greater than zero");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
            return false;
        }
        queue_.push_back(std::move(item));
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::deque<T> queue_;
    bool closed_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

}  // namespace concurx
