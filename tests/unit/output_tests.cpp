#include "output.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using rank_queries::Answer;
using rank_queries::CountAnswer;
using rank_queries::Key;
using rank_queries::KeyAnswer;
using rank_queries::OutOfRange;

struct OutputCase {
    std::vector<Answer> answers;
    std::string out;
    std::string errors;
    std::size_t error_count;
};

void check(const OutputCase& test_case) {
    std::ostringstream out;
    std::ostringstream errors;

    const auto error_count = rank_queries::output(test_case.answers, out, errors);

    EXPECT_EQ(out.str(), test_case.out);
    EXPECT_EQ(errors.str(), test_case.errors);
    EXPECT_EQ(error_count, test_case.error_count);
}

TEST(OutputTest, NoAnswersGiveEmptyOutput) {
    check({{}, "", "", 0});
}

TEST(OutputTest, SingleAnswer) {
    check({{CountAnswer{3}}, "3\n", "", 0});
    check({{KeyAnswer{-7}}, "-7\n", "", 0});
}

TEST(OutputTest, AnswersSeparatedBySpaces) {
    check({{KeyAnswer{-1}, KeyAnswer{2}, CountAnswer{2}, CountAnswer{2}}, "-1 2 2 2\n", "", 0});
}

TEST(OutputTest, ExtremeValues) {
    constexpr Key min = std::numeric_limits<Key>::min();
    constexpr Key max = std::numeric_limits<Key>::max();
    constexpr std::size_t max_count = std::numeric_limits<std::size_t>::max();

    check({{KeyAnswer{min}, KeyAnswer{max}, CountAnswer{max_count}},
           std::to_string(min) + " " + std::to_string(max) + " " + std::to_string(max_count) + "\n",
           "",
           0});
}

TEST(OutputTest, OnlyOutOfRangeGivesEmptyOutput) {
    check({{OutOfRange{0}, OutOfRange{4}},
           "",
           "Error: Index 0 is out of range\nError: Index 4 is out of range\n",
           2});
}

TEST(OutputTest, OutOfRangeAddsNoExtraSpaces) {
    const std::string error = "Error: Index 9 is out of range\n";

    check({{OutOfRange{9}, KeyAnswer{5}, CountAnswer{1}}, "5 1\n", error, 1});
    check({{KeyAnswer{5}, OutOfRange{9}, CountAnswer{1}}, "5 1\n", error, 1});
    check({{KeyAnswer{5}, CountAnswer{1}, OutOfRange{9}}, "5 1\n", error, 1});
}

} // namespace
