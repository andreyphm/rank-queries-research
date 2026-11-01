#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
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

    void insert(const KeyT& key) { root_ = insert_into(root_, key); }

    std::size_t count_less(const KeyT& key) const {
        return count_while([this, &key](const KeyT& node_key) { return comparator_(node_key, key); });
    }

    std::size_t count_in_range(const KeyT& low, const KeyT& high) const {
        if (!comparator_(low, high)) {
            return 0;
        }
        return count_less_equal(high) - count_less_equal(low);
    }

    std::optional<KeyT> kth_smallest(std::size_t i) const {
        const Node* node = root_;
        while (node) {
            const std::size_t left_size = size(node->left_);
            if (i <= left_size) {
                node = node->left_;
            } else if (i == left_size + 1) {
                return node->key_;
            } else {
                i -= left_size + 1;
                node = node->right_;
            }
        }
        return std::nullopt;
    }

private:
    struct Node {
        KeyT key_;
        Node* left_ = nullptr;
        Node* right_ = nullptr;
        int height_ = 1;
        std::size_t size_ = 1;

        explicit Node(const KeyT& key) : key_(key) {}
    };

    static int height(const Node* node) { return node ? node->height_ : 0; }
    static int balance_factor(const Node* node) { return height(node->left_) - height(node->right_); }
    static std::size_t size(const Node* node) { return node ? node->size_ : 0; }

    static void update(Node* node) {
        node->height_ = 1 + std::max(height(node->left_), height(node->right_));
        node->size_ = 1 + size(node->left_) + size(node->right_);
    }

    static Node* rotate_right(Node* y) {
        Node* x = y->left_;
        y->left_ = x->right_;
        x->right_ = y;
        update(y);
        update(x);
        return x;
    }

    static Node* rotate_left(Node* x) {
        Node* y = x->right_;
        x->right_ = y->left_;
        y->left_ = x;
        update(x);
        update(y);
        return y;
    }

    static Node* balance(Node* node) {
        update(node);
        const int factor = balance_factor(node);
        if (factor > 1) {
            if (balance_factor(node->left_) < 0) {
                node->left_ = rotate_left(node->left_);
            }
            return rotate_right(node);
        }
        if (factor < -1) {
            if (balance_factor(node->right_) > 0) {
                node->right_ = rotate_right(node->right_);
            }
            return rotate_left(node);
        }
        return node;
    }

    Node* insert_into(Node* node, const KeyT& key) {
        if (!node) {
            return new Node(key);
        }
        if (comparator_(key, node->key_)) {
            node->left_ = insert_into(node->left_, key);
        } else if (comparator_(node->key_, key)) {
            node->right_ = insert_into(node->right_, key);
        } else {
            return node;
        }
        return balance(node);
    }

    std::size_t count_less_equal(const KeyT& key) const {
        return count_while([this, &key](const KeyT& node_key) { return !comparator_(key, node_key); });
    }

    template <typename Predicate> std::size_t count_while(Predicate is_before) const {
        std::size_t count = 0;
        const Node* node = root_;
        while (node) {
            if (is_before(node->key_)) {
                count += size(node->left_) + 1;
                node = node->right_;
            } else {
                node = node->left_;
            }
        }
        return count;
    }

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
