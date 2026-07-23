
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
template <typename T>
class FrameQueue {
    private:
        std::deque<T> queue_;
        std::mutex mutex_;
        std::condition_variable not_empty_;
        std::condition_variable not_full_;

        size_t max_size_;
        std::atomic<bool> closed_{false};
    public:
        explicit FrameQueue(size_t max_size) : max_size_(max_size) {}
        void push(T item) {
            std::unique_lock<std::mutex> lock(mutex_);
            not_full_.wait(lock, [this]() {
                return queue_.size() < max_size_ || closed_;
            });
            if(closed_) return;

            queue_.push_back(std::move(item));

            not_empty_.notify_one();
        }
        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(mutex_);
            not_empty_.wait(lock, [this]() {
                return queue_.size() > 0 || closed_;
            });

            if(queue_.size() == 0 && closed_) return std::nullopt;

            T item = std::move(queue_.front());
            queue_.pop_front();


            not_full_.notify_one();

            return item;

        }
        void close() {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_.store(true, std::memory_order_relaxed);

            not_full_.notify_all();
            not_empty_.notify_all();
        }

        bool is_closed() const {
            return closed_.load(std::memory_order_relaxed);
        }

};
