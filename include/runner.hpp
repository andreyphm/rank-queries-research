#pragma once

#include "answer.hpp"
#include "query.hpp"

#include <utility>
#include <variant>
#include <vector>

namespace rank_queries {

template <typename Set> std::vector<Answer> run(const std::vector<Query>& queries) {
    struct Runner {
        Set set;
        std::vector<Answer> answers;

        void operator()(const Insert& query) {
            set.insert(query.key);
        }

        void operator()(const CountLess& query) {
            answers.push_back(CountAnswer{set.count_less(query.key)});
        }

        void operator()(const CountRange& query) {
            if (query.low >= query.high) {
                answers.push_back(CountAnswer{0});
                return;
            }
            answers.push_back(CountAnswer{set.count_less_equal(query.high) - set.count_less_equal(query.low)});
        }

        void operator()(const KthSmallest& query) {
            if (const auto key = set.kth_smallest(query.index)) {
                answers.push_back(KeyAnswer{*key});
            } else {
                answers.push_back(OutOfRange{query.index});
            }
        }
    };

    Runner runner;
    for (const auto& query : queries) {
        std::visit(runner, query);
    }
    return std::move(runner.answers);
}

} // namespace rank_queries
