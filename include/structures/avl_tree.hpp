#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

namespace rank_queries {

template <typename KeyT, typename Comparator = std::less<KeyT>> class AvlTree {
public:
    AvlTree() = default;

    AvlTree(const AvlTree& rhs) : comparator_(rhs.comparator_) { root_ = copy_tree(rhs.root_); }

    AvlTree& operator=(const AvlTree& rhs) {
        AvlTree copy(rhs);
        *this = std::move(copy);
        return *this;
    }

    AvlTree(AvlTree&& rhs) : root_(rhs.root_), comparator_(std::move(rhs.comparator_)) { rhs.root_ = nullptr; }

    AvlTree& operator=(AvlTree&& rhs) {
        std::swap(root_, rhs.root_);
        std::swap(comparator_, rhs.comparator_);
        return *this;
    }

    ~AvlTree() { destroy(root_); }

    void insert(const KeyT& key) {
        std::array<Node**, max_height> path;
        std::size_t depth = 0;
        Node** link = &root_;
        while (*link) {
            Node* node = *link;
            if (comparator_(key, node->key_)) {
                path[depth++] = link;
                link = &node->left_;
            } else if (comparator_(node->key_, key)) {
                path[depth++] = link;
                link = &node->right_;
            } else {
                return;
            }
        }
        *link = new Node(key);
        while (depth > 0) {
            Node** const parent_link = path[--depth];
            *parent_link = balance(*parent_link);
        }
    }

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

    static constexpr std::size_t max_height = std::numeric_limits<std::size_t>::digits * 3 / 2;

    static Node* copy_node(const Node* source) {
        Node* copy = new Node(source->key_);
        copy->height_ = source->height_;
        copy->size_ = source->size_;
        return copy;
    }

    static void fill_copy(const Node* source_root, Node* copy_root) {
        struct Pending {
            const Node* source;
            Node* copy;
        };
        std::array<Pending, max_height> pending;
        std::size_t count = 0;
        pending[count++] = {source_root, copy_root};
        while (count > 0) {
            const Pending current = pending[--count];
            if (current.source->left_) {
                current.copy->left_ = copy_node(current.source->left_);
                pending[count++] = {current.source->left_, current.copy->left_};
            }
            if (current.source->right_) {
                current.copy->right_ = copy_node(current.source->right_);
                pending[count++] = {current.source->right_, current.copy->right_};
            }
        }
    }

    static Node* copy_tree(const Node* source_root) {
        if (!source_root) {
            return nullptr;
        }
        Node* const copy_root = copy_node(source_root);
        try {
            fill_copy(source_root, copy_root);
        } catch (...) {
            destroy(copy_root);
            throw;
        }
        return copy_root;
    }

    static void destroy(Node* node) {
        while (node) {
            Node* left = node->left_;
            if (left) {
                node->left_ = left->right_;
                left->right_ = node;
                node = left;
            } else {
                Node* right = node->right_;
                delete node;
                node = right;
            }
        }
    }

    Node* root_ = nullptr;
    Comparator comparator_;
};

} // namespace rank_queries
