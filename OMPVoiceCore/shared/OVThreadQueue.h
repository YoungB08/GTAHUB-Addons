#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace ov
{
template <typename T>
class ThreadQueue final
{
public:
    explicit ThreadQueue(std::size_t capacity) : capacity_(capacity) {}

    bool Push(T value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_ || queue_.size() >= capacity_) return false;
        queue_.push_back(std::move(value));
        ready_.notify_one();
        return true;
    }

    std::optional<T> TryPop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop_front();
        return value;
    }

    void Stop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        ready_.notify_all();
    }

    [[nodiscard]] std::size_t Size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<T> queue_;
    bool stopped_{};
};
}
