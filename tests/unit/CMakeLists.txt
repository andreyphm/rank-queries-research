include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(unit_tests structure_tests.cpp parser_tests.cpp runner_tests.cpp)
target_link_libraries(unit_tests PRIVATE rank_queries_core GTest::gtest_main)
target_compile_options(unit_tests PRIVATE -Wall -Wextra -Wpedantic)
gtest_discover_tests(unit_tests)
