#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <set>

namespace rank_queries {

template <typename KeyT, typename Comparator = std::less<KeyT>> class SetWrapper {
public:
    void insert(const KeyT& key) { keys_.insert(key); }

    std::size_t count_less(const KeyT& key) const {
        return static_cast<std::size_t>(std::distance(keys_.begin(), keys_.lower_bound(key)));
    }

    std::size_t count_in_range(const KeyT& low, const KeyT& high) const {
        if (!keys_.key_comp()(low, high)) {
            return 0;
        }
        return static_cast<std::size_t>(std::distance(keys_.upper_bound(low), keys_.upper_bound(high)));
    }

    std::optional<KeyT> kth_smallest(std::size_t i) const {
        if (i == 0 || i > keys_.size()) {
            return std::nullopt;
        }
        return *std::next(keys_.begin(), static_cast<std::ptrdiff_t>(i - 1));
    }

private:
    std::set<KeyT, Comparator> keys_;
};

} // namespace rank_queries
