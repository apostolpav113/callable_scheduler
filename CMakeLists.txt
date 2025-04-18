cmake_minimum_required(VERSION 3.30)
project(scheduler)

set(CMAKE_CXX_STANDARD 20)

enable_testing()



add_executable(scheduler main.cpp
        callable_scheduler.h
        thread_pool.h)

add_subdirectory(tests)
