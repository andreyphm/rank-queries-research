#pragma once

#include <cstddef>
#include <functional>
#include <utility>

namespace rank_queries {

template <typename KeyT, typename Comparator = std::less<KeyT>> class AvlTree {
public:
    AvlTree() = default;

    AvlTree(const AvlTree&) = delete;
    AvlTree& operator=(const AvlTree&) = delete;

    AvlTree(AvlTree&& rhs) : root_(rhs.root_), comparator_(rhs.comparator_) { rhs.root_ = nullptr; }

    AvlTree& operator=(AvlTree&& rhs) {
        std::swap(root_, rhs.root_);
        std::swap(comparator_, rhs.comparator_);
        return *this;
    }

    ~AvlTree() { destroy(root_); }

private:
    struct Node {
        KeyT key_;
        Node* left_ = nullptr;
        Node* right_ = nullptr;
        int height_ = 1;
        std::size_t size_ = 1;

        explicit Node(const KeyT& key) : key_(key) {}
    };

    static void destroy(Node* node) {
        if (!node) {
            return;
        }
        destroy(node->left_);
        destroy(node->right_);
        delete node;
    }

    Node* root_ = nullptr;
    Comparator comparator_;
};

} // namespace rank_queries
