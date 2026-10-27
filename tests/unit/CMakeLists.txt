include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(unit_tests structure_tests.cpp)
target_include_directories(unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(unit_tests PRIVATE GTest::gtest_main)
target_compile_options(unit_tests PRIVATE -Wall -Wextra -Wpedantic)
gtest_discover_tests(unit_tests)
