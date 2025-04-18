#pragma once

#include <future>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace multi_threading {

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency()) {
    if (thread_count == 0) {
      throw std::runtime_error{"Thread count should be greater than 0"};
    }
    threads_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
      threads_.emplace_back(&ThreadPool::WorkerThread, this);
    }
  }

  template<typename Callable, typename ...Args, typename ResultType = std::invoke_result_t<Callable, Args...>>
  requires std::is_invocable_r_v<ResultType, Callable, Args...>
  std::optional<std::future<ResultType>> Enqueue(Callable&& callable, Args&&... args) {
    std::promise<ResultType> promise;
    auto future = promise.get_future();

    auto Task = [promise = std::move(promise), callable = std::forward<Callable>(callable),
        ...args = std::forward<Args>(args)] () mutable {
        try {
          if constexpr (std::is_void_v<ResultType>) {
            callable(std::forward<Args>(args)...);
            promise.set_value();
          } else {
            promise.set_value(callable(std::forward<Args>(args)...));
          }
        } catch (...) {
          promise.set_exception(std::current_exception());
        }
    };

    {
      std::lock_guard lock{mtx_};
      if (busy_threads_count_ == threads_.size()) {
        return {};
      }
      task_queue_.emplace(std::move(Task));
      ++busy_threads_count_;
    }
    cond_var_.notify_one();

    return future;
  }

 private:
  void WorkerThread(std::stop_token stop_token) {  // NOLINT
    while (!stop_token.stop_requested()) {
      std::packaged_task<void()> task;
      {
        std::unique_lock lock{mtx_};
        cond_var_.wait(lock, stop_token, [&]{ return !task_queue_.empty(); });
        if (stop_token.stop_requested()) break;
        task = std::move(task_queue_.front());
        task_queue_.pop();
      }
      task();  // TODO: should we do something with task self future?

      std::unique_lock lock{mtx_};  // TODO: do not like this moment...
      --busy_threads_count_;
    }
  }

  std::size_t busy_threads_count_ {0};
  std::queue<std::packaged_task<void()>> task_queue_;
  std::mutex mtx_;
  std::condition_variable_any cond_var_;
  std::vector<std::jthread> threads_;  // position is important - if it is above mtx_/cond_var_, then stop_token won't work
                                       // from inside the destructor (in the class default destructor there will be going
                                       // std::jthread's destructors to be called, where std::jthread::request_stop() && join() are called
                                       // and this won't work because in WorkerThread we are waiting on cond_var_ and mtx_,
                                       // and if they are destroyed first - then we have UB, actually)
};

}  // end of namespace multi_threading
