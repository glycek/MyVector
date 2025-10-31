cmake_minimum_required(VERSION 3.20)
project(MyVector)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_library(MyVector INTERFACE)

target_include_directories(MyVector INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(googletest)

add_executable(MyVectorTests
        test/tests.cpp
)

target_link_libraries(MyVectorTests PRIVATE
        MyVector
        GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(MyVectorTests)