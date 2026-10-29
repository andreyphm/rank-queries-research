#pragma once

#include "structures/avl_tree.hpp"
#include "structures/set_wrapper.hpp"
#include "structures/sorted_vector.hpp"

#include <gtest/gtest.h>

namespace rank_queries::tests {

using Structures = ::testing::Types<SortedVector<int>, SetWrapper<int>, AvlTree<int>>;

} // namespace rank_queries::tests
