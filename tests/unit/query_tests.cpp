#include "query.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {

using rank_queries::Key;

struct Describe {
    std::string operator()(const rank_queries::Insert& query) const {
        return "k " + std::to_string(query.key);
    }

    std::string operator()(const rank_queries::CountLess& query) const {
        return "n " + std::to_string(query.key);
    }

    std::string operator()(const rank_queries::CountRange& query) const {
        return "q " + std::to_string(query.low) + " " + std::to_string(query.high);
    }

    std::string operator()(const rank_queries::KthSmallest& query) const {
        return "m " + std::to_string(query.index);
    }
};

struct ParseCase {
    std::string input;
    std::vector<std::string> str_queries;
    std::string errors;
    std::size_t error_count;
};

void check(const ParseCase& test_case) {
    SCOPED_TRACE("input '" + test_case.input + "'");
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
    const auto min = std::to_string(std::numeric_limits<Key>::min());
    const auto max = std::to_string(std::numeric_limits<Key>::max());

    check({"k " + min + " k " + max, {"k " + min, "k " + max}, "", 0});
    check({"k " + max + "0", {}, "Error: Invalid query 'k " + max + "0'\n", 1});
    check({"k " + min + "0", {}, "Error: Invalid query 'k " + min + "0'\n", 1});
}

TEST(ParseTest, IndexBounds) {
    const auto max = std::to_string(std::numeric_limits<std::size_t>::max());

    check({"m 0", {"m 0"}, "", 0});
    check({"m " + max, {"m " + max}, "", 0});
    check({"m " + max + "0", {}, "Error: Invalid query 'm " + max + "0'\n", 1});
    check({"m -1", {}, "Error: Invalid query 'm -1'\n", 1});
}

TEST(ParseTest, InvalidNumbers) {
    for (const char* token : {"abc", "12abc", "7.5", "+3", "-", "0x10", "1e3"}) {
        const std::string query = std::string{"k "} + token;
        check({query, {}, "Error: Invalid query '" + query + "'\n", 1});
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
