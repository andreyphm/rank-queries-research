#pragma once

#include <cstddef>
#include <iosfwd>
#include <variant>
#include <vector>

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

struct ParseResult {
    std::vector<Query> queries;
    std::size_t error_count = 0;
};

ParseResult parse(std::istream& input, std::ostream& errors);

} // namespace rank_queries
