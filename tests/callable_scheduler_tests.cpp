#include <gtest/gtest.h>
#include <chrono>
#include <latch>

#include "../callable_scheduler.h"

TEST(CallableSchedulerTest, ShouldExecuteSimpleTask) {
  multi_threading::CallableScheduler scheduler;

  std::latch latch{1};

  using namespace std::chrono_literals;
  const auto start_time_point = std::chrono::system_clock::now();
  scheduler.Schedule([&]{ latch.count_down(); }, std::chrono::system_clock::now() + 1s);

  latch.wait();
  const auto time_elapsed = std::chrono::system_clock::now() - start_time_point;

  EXPECT_TRUE(time_elapsed >= 1s);
}

TEST(CallableSchedulerTest, ShouldRunIfInThePast) {
  multi_threading::CallableScheduler scheduler;

  std::latch latch{1};

  using namespace std::chrono_literals;
  const auto start_time_point = std::chrono::system_clock::now();
  EXPECT_NO_THROW(scheduler.Schedule([&]{ latch.count_down(); }, std::chrono::system_clock::now() - 10h));

  latch.wait();
  const auto time_elapsed = std::chrono::system_clock::now() - start_time_point;

  EXPECT_TRUE(time_elapsed < 1min);
}

TEST(CallableSchedulerTest, ShouldThrowIfInThePast) {
  multi_threading::CallableScheduler<multi_threading::ThrowOnExpiredTimePoint<>> scheduler;
  using namespace std::chrono_literals;
  auto when_to_start = std::chrono::system_clock::now() - 10h;
  EXPECT_THROW(scheduler.Schedule([&] {}, when_to_start), std::runtime_error);

  // check exception message
  try {
    scheduler.Schedule([&] {}, when_to_start);
    // should not go here
    GTEST_FAIL() << "Should throw an exception";
  } catch (const std::runtime_error& e) {
    const std::string expected_message = std::format("Time point '{}' has been expired already", when_to_start);
    EXPECT_EQ(expected_message, e.what());
  } catch (...) {
    // should not go here as well
    GTEST_FAIL() << "Should throw std::runtime_error";
  }
}

TEST(CallableSchedulerTest, ShouldRunOnNoFreeThreads) {
  // create a CallableScheduler instance containing internally a thread pool with only 1 worker thread
  multi_threading::CallableScheduler scheduler{1};

  std::binary_semaphore resume_long_task{0};
  std::atomic<bool> long_task_started = false;

  const std::size_t kTaskCount = 10;

  bool task_finished[kTaskCount] = {false};
  std::latch wait_all_latch{kTaskCount};

  // a long task - it is going to
  auto LongTask = [&] (bool& flag) {
      long_task_started = true;
      long_task_started.notify_one();

      resume_long_task.acquire();
      flag = true;

      wait_all_latch.count_down();
  };
  auto Task = [&] (bool& flag) {
    flag = true;
    wait_all_latch.count_down();
  };

  scheduler.Schedule(LongTask, std::chrono::system_clock::now(), std::reference_wrapper<bool>(task_finished[0]));
  for (std::size_t i = 1; i < kTaskCount; ++i){
    scheduler.Schedule(Task, std::chrono::system_clock::now(), std::reference_wrapper<bool>(task_finished[i]));
  }

  long_task_started.wait(false);
  EXPECT_TRUE(long_task_started);

  for (const auto& value : task_finished) {
    EXPECT_FALSE(value);
  }

  resume_long_task.release();
  wait_all_latch.wait();

  for (const auto& value : task_finished) {
    EXPECT_TRUE(value);
  }
}

TEST(CallableSchedulerTest, ShouldRunEarlierTaskFirst) {
  std::mutex mtx;
  std::string s;

  std::latch latch{3};

  auto Task = [&] (std::string_view substr_to_add) {
      {
        std::lock_guard lock{mtx};
        s += substr_to_add;
      }
      latch.count_down();
  };

  multi_threading::CallableScheduler scheduler;
  using namespace std::chrono_literals;
  const auto now = std::chrono::system_clock::now();
  scheduler.Schedule(Task, now + 2s, std::string{"-one-"});
  scheduler.Schedule(Task, now + 3s, std::string{"-two-"});
  scheduler.Schedule(Task, now, std::string{"-three-"});


  latch.wait();
  EXPECT_EQ(s, "-three--one--two-");
}