#include <gtest/gtest.h>
#include <latch>
#include <numeric>

#include "../thread_pool.h"

TEST(ThreadPool, ShouldThrowOnZeroThreads) {
  EXPECT_THROW({multi_threading::ThreadPool thread_pool{0};}, std::runtime_error);
  // check message
  try {
    multi_threading::ThreadPool thread_pool{0};
    GTEST_FAIL() << "Should have thrown an exception";  // should not be here
  } catch (const std::runtime_error& e) {
    const std::string kExpectedMessage = "Thread count should be greater than 0";
    EXPECT_EQ(kExpectedMessage, e.what());
  } catch (...) {
    GTEST_FAIL() << "Should have thrown std::runtime_error";  // should not be here as well
  }
}

TEST(ThreadPoolTest, ShouldRunSimpleTask) {
  multi_threading::ThreadPool thread_pool;
  auto opt_future = thread_pool.Enqueue([]{ return true; });
  EXPECT_TRUE(opt_future.has_value());
  EXPECT_TRUE(opt_future->get());
}

TEST(ThreadPoolTest, ShouldRefuseOnAllThreadsBusy) {
  multi_threading::ThreadPool thread_pool{1};

  std::latch stop_thread_latch{1};

  auto opt_future1 = thread_pool.Enqueue([&]{
      stop_thread_latch.wait();
      return true;
  });
  EXPECT_TRUE(opt_future1.has_value());

  auto opt_future2 = thread_pool.Enqueue([]{ return true; });
  EXPECT_FALSE(opt_future2.has_value());

  stop_thread_latch.count_down();
  EXPECT_TRUE(opt_future1->get());
}

TEST(ThreadPoolTest, CalculateSumOfArray) {
  const std::array<int, 12> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  auto CalculateTask = [] (std::span<const int> data) {
    return std::accumulate(data.begin(), data.end(), 0);
  };

  multi_threading::ThreadPool thread_pool{4};

  auto f1 = thread_pool.Enqueue(CalculateTask, std::span{arr.begin(), 3});
  auto f2 = thread_pool.Enqueue(CalculateTask, std::span{arr.begin() + 3, 3});
  auto f3 = thread_pool.Enqueue(CalculateTask, std::span{arr.begin() + 6, 3});
  auto f4 = thread_pool.Enqueue(CalculateTask, std::span{arr.begin() + 9, 3});

  EXPECT_TRUE(f1.has_value());
  EXPECT_TRUE(f2.has_value());
  EXPECT_TRUE(f3.has_value());
  EXPECT_TRUE(f4.has_value());

  const auto sum = f1->get() + f2->get() + f3->get() + f4->get();
  EXPECT_EQ(sum, std::accumulate(arr.begin(), arr.end(), 0));
}