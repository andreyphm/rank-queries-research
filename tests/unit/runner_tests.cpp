#include "runner.hpp"
#include "structures.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <format>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace {

using rank_queries::CountLess;
using rank_queries::CountRange;
using rank_queries::Insert;
using rank_queries::Key;
using rank_queries::KthSmallest;
using rank_queries::Query;

struct Describe {
    std::string operator()(const rank_queries::CountAnswer& answer) const {
        return std::format("count {}", answer.value);
    }

    std::string operator()(const rank_queries::KeyAnswer& answer) const {
        return std::format("key {}", answer.value);
    }

    std::string operator()(const rank_queries::OutOfRange& answer) const {
        return std::format("out of range {}", answer.index);
    }
};

std::vector<std::string> describe(const std::vector<rank_queries::Answer>& answers) {
    std::vector<std::string> str_answers;
    for (const auto& answer : answers) {
        str_answers.push_back(std::visit(Describe{}, answer));
    }
    return str_answers;
}

template <typename RankSet> std::vector<std::string> run_and_describe(const std::vector<Query>& queries) {
    return describe(rank_queries::run<RankSet>(queries));
}

template <typename RankSet> class RunnerTest : public ::testing::Test {};

TYPED_TEST_SUITE(RunnerTest, rank_queries::tests::Structures);

TYPED_TEST(RunnerTest, NoQueriesGiveNoAnswers) {
    EXPECT_TRUE(run_and_describe<TypeParam>({}).empty());
}

TYPED_TEST(RunnerTest, InsertGivesNoAnswer) {
    EXPECT_TRUE(run_and_describe<TypeParam>({Insert{1}, Insert{2}, Insert{1}}).empty());
}

TYPED_TEST(RunnerTest, EveryQueryKind) {
    const std::vector<Query> queries = {
        Insert{8},         Insert{2},         Insert{-1},       KthSmallest{1}, KthSmallest{2}, CountLess{3},
        CountRange{-1, 8}, CountRange{8, -1}, CountRange{2, 2}, KthSmallest{0}, KthSmallest{4},
    };
    const std::vector<std::string> expected = {
        "key -1", "key 2", "count 2", "count 2", "count 0", "count 0", "out of range 0", "out of range 4",
    };
    EXPECT_EQ(run_and_describe<TypeParam>(queries), expected);
}

TYPED_TEST(RunnerTest, QueriesOnEmptySet) {
    const std::vector<std::string> expected = {"count 0", "count 0", "out of range 1"};
    EXPECT_EQ(run_and_describe<TypeParam>({CountLess{0}, CountRange{-5, 5}, KthSmallest{1}}), expected);
}

TYPED_TEST(RunnerTest, RangeExcludesLowAndIncludesHigh) {
    const std::vector<Query> queries = {
        Insert{1}, Insert{2}, Insert{3}, CountRange{1, 3}, CountRange{0, 3}, CountRange{1, 2}, CountRange{3, 4},
    };
    const std::vector<std::string> expected = {"count 2", "count 3", "count 1", "count 0"};
    EXPECT_EQ(run_and_describe<TypeParam>(queries), expected);
}

TYPED_TEST(RunnerTest, EmptyOrReversedRangeGivesZero) {
    const std::vector<Query> queries = {
        Insert{1}, Insert{2}, Insert{3}, CountRange{2, 2}, CountRange{3, 1}, CountRange{3, 0},
    };
    const std::vector<std::string> expected = {"count 0", "count 0", "count 0"};
    EXPECT_EQ(run_and_describe<TypeParam>(queries), expected);
}

TYPED_TEST(RunnerTest, RangeOverExtremeKeys) {
    constexpr Key min = std::numeric_limits<Key>::min();
    constexpr Key max = std::numeric_limits<Key>::max();
    const std::vector<Query> queries = {
        Insert{min}, Insert{0}, Insert{max}, CountRange{min, max}, CountRange{max, min}, CountLess{max},
    };
    const std::vector<std::string> expected = {"count 2", "count 0", "count 2"};
    EXPECT_EQ(run_and_describe<TypeParam>(queries), expected);
}

TYPED_TEST(RunnerTest, KthSmallestOutOfRange) {
    constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
    const std::vector<Query> queries = {
        Insert{5}, Insert{7}, KthSmallest{0}, KthSmallest{2}, KthSmallest{3}, KthSmallest{max},
    };
    const std::vector<std::string> expected = {
        "out of range 0", "key 7", "out of range 3", std::format("out of range {}", max),
    };
    EXPECT_EQ(run_and_describe<TypeParam>(queries), expected);
}

TYPED_TEST(RunnerTest, AnswersSeeOnlyEarlierInserts) {
    const std::vector<Query> queries = {
        CountLess{5}, Insert{1}, CountLess{5}, Insert{3}, CountLess{5}, KthSmallest{2}, Insert{2}, KthSmallest{2},
    };
    const std::vector<std::string> expected = {"count 0", "count 1", "count 2", "key 3", "key 2"};
    EXPECT_EQ(run_and_describe<TypeParam>(queries), expected);
}

TYPED_TEST(RunnerTest, SecondRunContinuesOnTheSameSet) {
    TypeParam rank_set;
    EXPECT_TRUE(rank_queries::run(rank_set, {Insert{5}, Insert{1}, Insert{9}}).empty());

    const std::vector<Query> queries = {CountLess{3}, Insert{7}, KthSmallest{3}};
    const std::vector<std::string> expected = {"count 1", "key 7"};
    EXPECT_EQ(describe(rank_queries::run(rank_set, queries)), expected);
}

} // namespace
