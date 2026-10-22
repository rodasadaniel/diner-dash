cmake_minimum_required(VERSION 3.16)
project(diner_dash VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(diner_lib
  src/scheduler.cpp
  src/stats.cpp
  src/workload.cpp
  src/simulation.cpp
)
target_include_directories(diner_lib PUBLIC include)
target_compile_options(diner_lib PRIVATE -Wall -Wextra)

add_executable(diner_dash src/main.cpp)
target_link_libraries(diner_dash PRIVATE diner_lib)

option(DINER_BUILD_TESTS "Build the GoogleTest suite" ON)
if(DINER_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(diner_tests
    tests/test_stats.cpp
    tests/test_scheduler.cpp
    tests/test_workload.cpp
    tests/test_simulation.cpp
  )
  target_link_libraries(diner_tests PRIVATE diner_lib GTest::gtest_main)
  target_compile_options(diner_tests PRIVATE -Wall -Wextra)

  include(GoogleTest)
  gtest_discover_tests(diner_tests)
endif()
