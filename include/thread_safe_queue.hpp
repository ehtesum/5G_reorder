#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace pdcp {

template <typename T>
class ThreadSafeQueue {
private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    const size_t max_capacity_;
    std::atomic<bool> shutdown_{false};
    std::atomic<uint64_t> drops_{0};

public:
    explicit ThreadSafeQueue(size_t capacity = 4096)
        : max_capacity_(capacity) {}

    ~ThreadSafeQueue() {
        shutdown();
    }

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    bool push(T item) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (shutdown_.load() || queue_.size() >= max_capacity_) {
                drops_++;
                return false;
            }
            queue_.push(std::move(item));
        }
        cv_.notify_one();
        return true;
    }

    bool pop_wait(T& item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || shutdown_.load(); })) {
            return false;
        }
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            shutdown_.store(true);
        }
        cv_.notify_all();
    }

    bool is_shutdown() const { return shutdown_.load(); }
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }
    uint64_t drop_count() const { return drops_.load(); }
};

} // namespace pdcp
