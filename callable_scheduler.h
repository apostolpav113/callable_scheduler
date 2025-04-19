#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

#include "thread_pool.h"


namespace multi_threading {

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

template<typename T>
concept ScheduleExpiredTimePointPolicy = std::is_invocable_v<T, TimePoint> && requires (T t, TimePoint time_point) {
  { t(time_point) } -> std::same_as<void>;
};

struct AllowExpiredTimePoint {
  void operator()(TimePoint) {}
};

template<typename Clock = std::chrono::system_clock,
    std::derived_from<std::exception> Exception = std::runtime_error,
    typename TimePoint = std::chrono::time_point<Clock>>
struct ThrowOnExpiredTimePoint {
  void operator()(TimePoint p) {
    const auto now = Clock::now();
    if (p < now) {
      throw Exception{std::format("Time point '{}' has been expired already", p)};
    }
  }
};


template<ScheduleExpiredTimePointPolicy ExpiredTimePointPolicy = AllowExpiredTimePoint>
class CallableScheduler {
 public:
  explicit CallableScheduler(std::size_t thread_count = std::thread::hardware_concurrency())
      : thread_pool_(thread_count) {
    executor_thread_ = std::jthread(&CallableScheduler::ExecutorFunc, this);
  }

  template<typename Callable, typename ...Args>
  requires std::is_invocable_v<Callable, Args...>
  void Schedule(Callable&& callable, TimePoint when, Args&&... args) {
    expired_time_point_policy_(when);

    auto EnqueueTask = [this, callable = std::forward<Callable>(callable),
        ...args = std::forward<Args>(args)] () mutable {
      const auto rs = thread_pool_.Enqueue(std::forward<Callable>(callable), std::forward<Args>(args)...);
      return rs.has_value();
    };

    {
      std::lock_guard lock{mtx_};
      queue_.emplace(when, std::move(EnqueueTask));
      cond_var_.notify_one();
    }
  }

 private:
  using QueueItem = std::pair<TimePoint, std::function<bool()>>;

  struct QueueItemCompare {
    bool operator()(const QueueItem& left, const QueueItem& right) const {
      return left.first > right.first;
    }
  };

  void ExecutorFunc(std::stop_token stop_token) {
    QueueItem current_item;
    bool wait_result = false;
    while (!stop_token.stop_requested()) {
      {
        std::unique_lock lock{mtx_};
        if (current_item.second) {
          queue_.emplace(std::move(current_item));
        }
        cond_var_.wait(lock, stop_token, [&]() { return !queue_.empty(); });
        if (stop_token.stop_requested()) break;
        current_item = std::move(queue_.top());
        queue_.pop();
        const auto when = current_item.first;
        wait_result = cond_var_.wait_until(lock, stop_token, when, [&](){
            const auto now = std::chrono::system_clock::now();
            return now >= when && (queue_.empty() || queue_.top().first >= when);
        });
      }

      if (wait_result) {
        const auto enqueue_in_threadpool_func = current_item.second;
        const bool enqueued = enqueue_in_threadpool_func();
        if (enqueued) {
          current_item = {};
          continue;
        }
        // otherwise, all threads are busy, and we should put the task back in the queue
        // Note: this way we can run into a spinning loop (i.e. putting the same task into the queue, and immediately
        //       taking it out of the queue on the next step)
        // TODO: make ThreadPool class to notify if there are free threads, this way we would be able to wait on
        //       the cond_var_ like this:
        //       cond_var_.wait(lock, stop_token, [&](){ return !queue_.empty() && thread_pool_.has_free_threads(); });
      }
    }
  }

  multi_threading::ThreadPool thread_pool_;
  std::priority_queue<QueueItem, std::vector<QueueItem>, QueueItemCompare> queue_;
  std::mutex mtx_;
  std::condition_variable_any cond_var_;
  std::jthread executor_thread_;

  [[no_unique_address]] ExpiredTimePointPolicy expired_time_point_policy_;
};

}  // namespace multi_threading
