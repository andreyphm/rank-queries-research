#pragma once

#include "avl_tree.hpp"
#include "set_wrapper.hpp"
#include "sorted_vector.hpp"

#include <gtest/gtest.h>

namespace rank_queries::tests {

using Structures = ::testing::Types<SortedVector<int>, SetWrapper<int>, AvlTree<int>>;

} // namespace rank_queries::tests
