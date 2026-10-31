#pragma once

#include "structures/avl_tree.hpp"
#include "structures/btree.hpp"
#include "structures/set_wrapper.hpp"
#include "structures/sorted_vector.hpp"

#include <gtest/gtest.h>

#include <functional>

namespace rank_queries::tests {

using Structures = ::testing::Types<SortedVector<int>, SetWrapper<int>, AvlTree<int>, BTree<int, std::less<int>, 2>>;

} // namespace rank_queries::tests
