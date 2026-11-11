#include "parser.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <format>
#include <limits>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {

using rank_queries::Key;

struct Describe {
    std::string operator()(const rank_queries::Insert& query) const {
        return std::format("k {}", query.key);
    }

    std::string operator()(const rank_queries::CountLess& query) const {
        return std::format("n {}", query.key);
    }

    std::string operator()(const rank_queries::CountRange& query) const {
        return std::format("q {} {}", query.low, query.high);
    }

    std::string operator()(const rank_queries::KthSmallest& query) const {
        return std::format("m {}", query.index);
    }
};

struct ParseCase {
    std::string input;
    std::vector<std::string> str_queries;
    std::string errors;
    std::size_t error_count;
};

void check(const ParseCase& test_case) {
    SCOPED_TRACE(std::format("input '{}'", test_case.input));
    std::istringstream input{test_case.input};
    std::ostringstream errors;

    const auto result = rank_queries::parse(input, errors);

    std::vector<std::string> str_queries;
    for (const auto& query : result.queries) {
        str_queries.push_back(std::visit(Describe{}, query));
    }
    EXPECT_EQ(str_queries, test_case.str_queries);
    EXPECT_EQ(errors.str(), test_case.errors);
    EXPECT_EQ(result.error_count, test_case.error_count);
}

TEST(ParseTest, EmptyInput) {
    check({"", {}, "", 0});
    check({" \n\t\r\n", {}, "", 0});
}

TEST(ParseTest, EveryCommand) {
    check({"k 8 n 3 q -1 8 m 2", {"k 8", "n 3", "q -1 8", "m 2"}, "", 0});
}

TEST(ParseTest, AnyWhitespaceSeparatesTokens) {
    check({"  k\t8\n\nq\n-1   8\r\nm 1\n", {"k 8", "q -1 8", "m 1"}, "", 0});
}

TEST(ParseTest, KeyBounds) {
    constexpr Key min = std::numeric_limits<Key>::min();
    constexpr Key max = std::numeric_limits<Key>::max();

    check({std::format("k {} k {}", min, max), {std::format("k {}", min), std::format("k {}", max)}, "", 0});
    check({std::format("k {}0", max), {}, std::format("Error: Invalid query 'k {}0'\n", max), 1});
    check({std::format("k {}0", min), {}, std::format("Error: Invalid query 'k {}0'\n", min), 1});
}

TEST(ParseTest, IndexBounds) {
    constexpr std::size_t max = std::numeric_limits<std::size_t>::max();

    check({"m 0", {"m 0"}, "", 0});
    check({std::format("m {}", max), {std::format("m {}", max)}, "", 0});
    check({std::format("m {}0", max), {}, std::format("Error: Invalid query 'm {}0'\n", max), 1});
    check({"m -1", {}, "Error: Invalid query 'm -1'\n", 1});
}

TEST(ParseTest, InvalidNumbers) {
    for (const char* token : {"abc", "12abc", "7.5", "+3", "-", "0x10", "1e3"}) {
        const std::string query = std::format("k {}", token);
        check({query, {}, std::format("Error: Invalid query '{}'\n", query), 1});
    }
}

TEST(ParseTest, UnknownCommands) {
    check({"x", {}, "Error: Unknown command 'x'\n", 1});
    check({"K 5", {}, "Error: Unknown command 'K'\nError: Unknown command '5'\n", 2});
    check({"kk 5", {}, "Error: Unknown command 'kk'\nError: Unknown command '5'\n", 2});
    check({"k5", {}, "Error: Unknown command 'k5'\n", 1});
}

TEST(ParseTest, RangeQueryConsumesBothArguments) {
    check({"q abc 7 k 1", {"k 1"}, "Error: Invalid query 'q abc 7'\n", 1});
    check({"q 1 x k 1", {"k 1"}, "Error: Invalid query 'q 1 x'\n", 1});
}

TEST(ParseTest, MissingArgumentAtEnd) {
    check({"k", {}, "Error: Missing argument in 'k'\n", 1});
    check({"k 1 m", {"k 1"}, "Error: Missing argument in 'm'\n", 1});
    check({"q 1", {}, "Error: Missing argument in 'q 1'\n", 1});
    check({"q abc", {}, "Error: Missing argument in 'q abc'\n", 1});
}

TEST(ParseTest, NextCommandTakenAsArgument) {
    check({"k n 5", {}, "Error: Invalid query 'k n'\nError: Unknown command '5'\n", 2});
}

TEST(ParseTest, ErrorsDoNotStopParsing) {
    check({"k 1 x k 2 k abc k 3",
           {"k 1", "k 2", "k 3"},
           "Error: Unknown command 'x'\nError: Invalid query 'k abc'\n",
           2});
}

} // namespace
