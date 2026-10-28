#pragma once

#include "query.hpp"

#include <cstddef>
#include <variant>

namespace rank_queries {

struct CountAnswer {
    std::size_t value;
};

struct KeyAnswer {
    Key value;
};

struct OutOfRange {
    std::size_t index;
};

using Answer = std::variant<CountAnswer, KeyAnswer, OutOfRange>;

} // namespace rank_queries
