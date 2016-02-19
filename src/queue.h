#pragma once
#include <chrono>
#include <cstddef>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace mongo_smasher {
  
//
// Queue is a concurrent multi-producers multi-readers queue
//
// This queue is used to dispatch the produced documents to
// the writer threads.
template <class T> class Queue {
  size_t size_ { 0u };
  size_t max_size_ { 100000u };
  size_t waiting_readers_ {0};
  size_t waiting_writers_ {0};
  mutable std::mutex main_lock_;
  std::condition_variable push_guard_;
  std::condition_variable pop_guard_;
  std::queue<T> values_;

 public:
  using duration_t = std::chrono::duration<size_t>;
  using wait_clock_t = std::chrono::high_resolution_clock;
  using time_point_t = std::chrono::time_point<wait_clock_t>;

  Queue() = default;
  Queue(size_t max_size) : max_size_{max_size} {}
  Queue(Queue&&) = default;

  size_t size() const {
    std::lock_guard<std::mutex> lock(main_lock_);
    return size_;
  }

  // 
  // Pushes a value to be read on the queue.
  //
  // Since the funtion can block if the queue is full or busy,
  // it returns the time taken to push.
  duration_t push(T&& value) {
    std::unique_lock<std::mutex> lock(main_lock_);
    
    // If queue is full, we wait
    duration_t wait_time {0u};
    if (max_size_ <= size_) {
      ++waiting_writers_;
      time_point_t start = wait_clock_t::now();
      push_guard_.wait(lock, [this]() { return this->size_ < this->max_size_; });
      wait_time = std::chrono::duration_cast<duration_t>(wait_clock_t::now() - start);
      --waiting_writers_;
    }

    // Add to the queue
    values_.push(std::move(value));
    ++size_;

    // Help another potential waiting thread to be unlocked
    if (waiting_readers_) {
      lock.unlock();
      pop_guard_.notify_one();
    }
    else if (size_ < max_size_ - 1 && waiting_writers_) {
      lock.unlock();
      push_guard_.notify_one();
    }

    return wait_time;
  }

  // 
  // Pops a value on the queue.
  //
  // The function blocks if there is nothing to consume. Can take an optional
  // duration meant to be affected in case of block.
  T pop(duration_t* block_duration = nullptr) {
    std::unique_lock<std::mutex> lock(main_lock_);

    // if queue is empty, we wait
    if (block_duration)
      *block_duration = duration_t {0u};
    if (0u == size_) {
      ++waiting_readers_;
      time_point_t start = wait_clock_t::now();
      pop_guard_.wait(lock,[this]() { return 0u < this->size_;});
      if (block_duration)
        *block_duration = std::chrono::duration_cast<duration_t>(wait_clock_t::now() - start);
      --waiting_readers_;
    }

    // Pops the element
    T value { std::move(values_.front()) };
    values_.pop();
    --size_;

    if (0u < size_ && waiting_readers_) {
      lock.unlock();
      pop_guard_.notify_one();
    }
    else if (waiting_writers_) {
      lock.unlock();
      push_guard_.notify_one();
    }

    return value;
  }
};


}  // namespace mongo_smasher
