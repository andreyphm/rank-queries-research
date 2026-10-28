#pragma once

#include "answer.hpp"

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace rank_queries {

std::size_t output(const std::vector<Answer>& answers, std::ostream& out, std::ostream& errors);

} // namespace rank_queries
