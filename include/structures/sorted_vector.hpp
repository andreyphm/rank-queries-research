#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace rank_queries {

template <typename KeyT, typename Comparator = std::less<KeyT>> class SortedVector {
public:
    void insert(const KeyT& key) {
        const auto pos = std::lower_bound(keys_.begin(), keys_.end(), key, comparator_);
        if (pos == keys_.end() || comparator_(key, *pos)) {
            keys_.insert(pos, key);
        }
    }

    std::size_t count_less(const KeyT& key) const {
        const auto pos = std::lower_bound(keys_.begin(), keys_.end(), key, comparator_);
        return static_cast<std::size_t>(pos - keys_.begin());
    }

    std::size_t count_less_equal(const KeyT& key) const {
        const auto pos = std::upper_bound(keys_.begin(), keys_.end(), key, comparator_);
        return static_cast<std::size_t>(pos - keys_.begin());
    }

    std::optional<KeyT> kth_smallest(std::size_t i) const {
        if (i == 0 || i > keys_.size()) {
            return std::nullopt;
        }
        return keys_[i - 1];
    }

private:
    std::vector<KeyT> keys_;
    Comparator comparator_;
};

} // namespace rank_queries
