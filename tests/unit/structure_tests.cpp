#include "set_wrapper.hpp"
#include "sorted_vector.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace {

template <typename Set> class StructureTest : public ::testing::Test {};

using Structures = ::testing::Types<rank_queries::SortedVector<int>, rank_queries::SetWrapper<int>>;
TYPED_TEST_SUITE(StructureTest, Structures);

struct CountCase {
    int key;
    std::size_t less;
    std::size_t less_equal;
};

TYPED_TEST(StructureTest, EmptySetHasNoKeys) {
    const TypeParam set{};

    EXPECT_EQ(set.count_less(0), 0u);
    EXPECT_EQ(set.count_less_equal(0), 0u);
    EXPECT_FALSE(set.kth_smallest(0).has_value());
    EXPECT_FALSE(set.kth_smallest(1).has_value());
}

TYPED_TEST(StructureTest, SingleKey) {
    TypeParam set;
    set.insert(5);

    const CountCase cases[] = {{4, 0, 0}, {5, 0, 1}, {6, 1, 1}};
    for (const auto& c : cases) {
        EXPECT_EQ(set.count_less(c.key), c.less) << "key " << c.key;
        EXPECT_EQ(set.count_less_equal(c.key), c.less_equal) << "key " << c.key;
    }
    EXPECT_EQ(set.kth_smallest(1), 5);
    EXPECT_FALSE(set.kth_smallest(2).has_value());
}

TYPED_TEST(StructureTest, DuplicateInsertIsIgnored) {
    TypeParam set;
    set.insert(5);
    set.insert(3);
    set.insert(5);
    set.insert(3);

    EXPECT_EQ(set.count_less(5), 1u);
    EXPECT_EQ(set.count_less_equal(5), 2u);
    EXPECT_EQ(set.kth_smallest(1), 3);
    EXPECT_EQ(set.kth_smallest(2), 5);
    EXPECT_FALSE(set.kth_smallest(3).has_value());
}

TYPED_TEST(StructureTest, CountsBelowOnBetweenAndAboveKeys) {
    TypeParam set;
    set.insert(10);
    set.insert(-10);
    set.insert(0);

    const CountCase cases[] = {
        {-11, 0, 0}, {-10, 0, 1}, {-5, 1, 1}, {0, 1, 2},
        {5, 2, 2},   {10, 2, 3},  {11, 3, 3},
    };
    for (const auto& c : cases) {
        EXPECT_EQ(set.count_less(c.key), c.less) << "key " << c.key;
        EXPECT_EQ(set.count_less_equal(c.key), c.less_equal) << "key " << c.key;
    }
}

TYPED_TEST(StructureTest, KthSmallestCoversEveryPosition) {
    TypeParam set;
    for (const int key : {30, -20, 10, 0, -40}) {
        set.insert(key);
    }

    const int sorted[] = {-40, -20, 0, 10, 30};
    for (std::size_t i = 1; i <= std::size(sorted); ++i) {
        EXPECT_EQ(set.kth_smallest(i), sorted[i - 1]) << "i " << i;
    }
    EXPECT_FALSE(set.kth_smallest(0).has_value());
    EXPECT_FALSE(set.kth_smallest(std::size(sorted) + 1).has_value());
}

TYPED_TEST(StructureTest, ExtremeKeys) {
    constexpr int min = std::numeric_limits<int>::min();
    constexpr int max = std::numeric_limits<int>::max();
    TypeParam set;
    set.insert(max);
    set.insert(0);
    set.insert(min);

    const CountCase cases[] = {{min, 0, 1}, {0, 1, 2}, {max, 2, 3}};
    for (const auto& c : cases) {
        EXPECT_EQ(set.count_less(c.key), c.less) << "key " << c.key;
        EXPECT_EQ(set.count_less_equal(c.key), c.less_equal) << "key " << c.key;
    }
    EXPECT_EQ(set.kth_smallest(1), min);
    EXPECT_EQ(set.kth_smallest(3), max);
}

TYPED_TEST(StructureTest, InsertionOrderDoesNotMatter) {
    constexpr int count = 200;
    std::vector<int> ascending(count);
    for (int i = 0; i < count; ++i) {
        ascending[i] = i + 1;
    }
    std::vector<int> descending(ascending.rbegin(), ascending.rend());
    std::vector<int> shuffled = ascending;
    std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937{1});

    for (const auto* order : {&ascending, &descending, &shuffled}) {
        TypeParam set;
        for (const int key : *order) {
            set.insert(key);
        }
        for (int key = 0; key <= count + 1; ++key) {
            const auto expected_less = static_cast<std::size_t>(std::clamp(key - 1, 0, count));
            const auto expected_less_equal = static_cast<std::size_t>(std::clamp(key, 0, count));
            EXPECT_EQ(set.count_less(key), expected_less) << "key " << key;
            EXPECT_EQ(set.count_less_equal(key), expected_less_equal) << "key " << key;
        }
        for (int i = 1; i <= count; ++i) {
            EXPECT_EQ(set.kth_smallest(static_cast<std::size_t>(i)), i) << "i " << i;
        }
        EXPECT_FALSE(set.kth_smallest(count + 1).has_value());
    }
}

TYPED_TEST(StructureTest, MatchesBruteForceOnRandomOperations) {
    std::mt19937 generator{42};
    std::uniform_int_distribution<int> key_distribution{-100, 100};
    TypeParam set;
    std::set<int> reference;

    for (int step = 0; step < 3000; ++step) {
        const int inserted = key_distribution(generator);
        set.insert(inserted);
        reference.insert(inserted);

        const int key = key_distribution(generator);
        const auto expected_less = static_cast<std::size_t>(
            std::count_if(reference.begin(), reference.end(), [key](int k) { return k < key; }));
        const auto expected_less_equal = static_cast<std::size_t>(
            std::count_if(reference.begin(), reference.end(), [key](int k) { return k <= key; }));
        ASSERT_EQ(set.count_less(key), expected_less) << "step " << step << ", key " << key;
        ASSERT_EQ(set.count_less_equal(key), expected_less_equal) << "step " << step << ", key " << key;

        std::uniform_int_distribution<std::size_t> index_distribution{0, reference.size() + 1};
        const std::size_t i = index_distribution(generator);
        if (i == 0 || i > reference.size()) {
            ASSERT_FALSE(set.kth_smallest(i).has_value()) << "step " << step << ", i " << i;
        } else {
            ASSERT_EQ(set.kth_smallest(i), *std::next(reference.begin(), static_cast<std::ptrdiff_t>(i - 1)))
                << "step " << step << ", i " << i;
        }
    }
}

} // namespace
