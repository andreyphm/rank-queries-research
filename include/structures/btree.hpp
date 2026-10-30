#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <utility>

namespace rank_queries {

template <typename KeyT, typename Comparator = std::less<KeyT>, std::size_t T = 32> class BTree {
    static_assert(T >= 2);

public:
    BTree() = default;

    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;

    BTree(BTree&& rhs) : root_(rhs.root_), comparator_(rhs.comparator_) { rhs.root_ = nullptr; }

    BTree& operator=(BTree&& rhs) {
        std::swap(root_, rhs.root_);
        std::swap(comparator_, rhs.comparator_);
        return *this;
    }

    ~BTree() { destroy(root_); }

private:
    struct Node {
        std::size_t key_count_ = 0;
        std::array<KeyT, 2*T - 1> keys_;
        std::array<Node*, 2*T> children_{};
        std::array<std::size_t, 2*T> child_sizes_{};
    };

    static void destroy(Node* node) {
        if (!node) {
            return;
        }
        for (std::size_t i = 0; i <= node->key_count_; ++i) {
            destroy(node->children_[i]);
        }
        delete node;
    }

    Node* root_ = nullptr;
    Comparator comparator_;
};

} // namespace rank_queries
