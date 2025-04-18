include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
        googletest
        # Specify the commit you depend on and update it regularly.
        GIT_REPOSITORY  https://github.com/google/googletest.git
        GIT_TAG         6910c9d9165801d8827d628cb72eb7ea9dd538c5
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(tests
        thread_pool_tests.cpp
        callable_scheduler_tests.cpp
)
target_link_libraries(tests gtest_main)

add_executable(thread_pool_tests
        thread_pool_tests.cpp
)
target_link_libraries(thread_pool_tests gtest_main)

add_executable(callable_scheduler_tests
        callable_scheduler_tests.cpp
)
target_link_libraries(callable_scheduler_tests gtest_main)

add_test(NAME thread_pool COMMAND thread_pool_tests)
add_test(NAME callable_scheduler COMMAND callable_scheduler_tests)
