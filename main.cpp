#include <chrono>
#include <iostream>
#include <syncstream>

#include "callable_scheduler.h"
#include "thread_pool.h"


int main() {
  multi_threading::CallableScheduler scheduler;
  // create thread_pool on the 2nd step; this way thread_pool object will wait in the destructor for its threads
  // termination preventing the scheduler object to be destroyed
  multi_threading::ThreadPool thread_pool{1};

  std::atomic_flag thread_started;

  thread_pool.Enqueue([&] {
      thread_started.test_and_set();
      thread_started.notify_one();
      for (int i = 0; i < 20; ++i) {
        {
          std::osyncstream osyncstream{std::cout};
          osyncstream << "\r" << i << "...";
          osyncstream.flush();
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
      }
      std::osyncstream osyncstream{std::cout};
      osyncstream << std::endl;
  });

  // wait for the thread to start, otherwise it is possible the situation when the thread_pool is being destroyed before
  // the thread actually starts
  thread_started.wait(false);

  using namespace std::chrono_literals;
  scheduler.Schedule([] {
      {
        std::osyncstream osyncstream{std::cout};
        osyncstream << "\nTerminate now..." << std::endl;
      }
      std::terminate();
  }, std::chrono::system_clock::now() + 10s);

  return 0;
}
