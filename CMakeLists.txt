cmake_minimum_required(VERSION 3.16)
project(rank_queries LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rank_queries.out source/rank_queries.cpp)
target_include_directories(rank_queries.out PRIVATE "include")
target_compile_options(rank_queries.out PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests/unit)
endif()
