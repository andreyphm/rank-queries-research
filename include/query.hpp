#pragma once

#include <cstddef>
#include <variant>

namespace rank_queries {

using Key = int;

struct Insert {
    Key key;
};

struct CountLess {
    Key key;
};

struct CountRange {
    Key low;
    Key high;
};

struct KthSmallest {
    std::size_t index;
};

using Query = std::variant<Insert, CountLess, CountRange, KthSmallest>;

} // namespace rank_queries
