cmake_minimum_required(VERSION 4.0)
project(scheduler)

set(CMAKE_CXX_STANDARD 20)

add_executable(scheduler main.cpp
        callable_scheduler.h
        thread_pool.h)

add_subdirectory(tests)
