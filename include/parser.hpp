#pragma once

#include "query.hpp"

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace rank_queries {

struct ParseResult {
    std::vector<Query> queries;
    std::size_t error_count = 0;
};

ParseResult parse(std::istream& input, std::ostream& errors);

} // namespace rank_queries
