#pragma once

#include "answer.hpp"
#include "query.hpp"

#include <variant>
#include <vector>

namespace rank_queries {

template <typename Set> std::vector<Answer> run(const std::vector<Query>& queries) {
    struct Runner {
        Set& set;
        std::vector<Answer>& answers;

        void operator()(const Insert& query) const {
            set.insert(query.key);
        }

        void operator()(const CountLess& query) const {
            answers.push_back(CountAnswer{set.count_less(query.key)});
        }

        void operator()(const CountRange& query) const {
            if (query.low >= query.high) {
                answers.push_back(CountAnswer{0});
                return;
            }
            answers.push_back(CountAnswer{set.count_less_equal(query.high) - set.count_less_equal(query.low)});
        }

        void operator()(const KthSmallest& query) const {
            if (const auto key = set.kth_smallest(query.index)) {
                answers.push_back(KeyAnswer{*key});
            } else {
                answers.push_back(OutOfRange{query.index});
            }
        }
    };

    Set set;
    std::vector<Answer> answers;
    const Runner runner{set, answers};
    for (const auto& query : queries) {
        std::visit(runner, query);
    }
    return answers;
}

} // namespace rank_queries
