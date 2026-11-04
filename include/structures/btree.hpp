#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>

namespace rank_queries {

template <typename KeyT, typename Comparator = std::less<KeyT>, std::size_t T = 32> class BTree {
    static_assert(T >= 2);

public:
    BTree() = default;

    BTree(const BTree& rhs) : comparator_(rhs.comparator_) { root_ = copy_tree(rhs.root_); }

    BTree& operator=(const BTree& rhs) {
        BTree copy(rhs);
        *this = std::move(copy);
        return *this;
    }

    BTree(BTree&& rhs) : root_(rhs.root_), comparator_(std::move(rhs.comparator_)) { rhs.root_ = nullptr; }

    BTree& operator=(BTree&& rhs) {
        std::swap(root_, rhs.root_);
        std::swap(comparator_, rhs.comparator_);
        return *this;
    }

    ~BTree() { destroy(root_); }

    void insert(const KeyT& key) {
        if (contains(key)) {
            return;
        }
        if (!root_) {
            root_ = new Node;
        }
        if (is_full(root_)) {
            Node* new_root = new Node;
            new_root->children_[0] = root_;
            root_ = new_root;
            split_child(new_root, 0);
        }

        Node* node = root_;
        while (!is_leaf(node)) {
            std::size_t i = keys_before(node, key);
            if (is_full(node->children_[i])) {
                split_child(node, i);
                if (comparator_(node->keys_[i], key)) {
                    ++i;
                }
            }
            ++node->child_sizes_[i];
            node = node->children_[i];
        }
        insert_key(node, keys_before(node, key), key);
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
            std::size_t child_index = 0;
            for (; child_index < node->key_count_; ++child_index) {
                const std::size_t child_size = node->child_sizes_[child_index];
                if (i <= child_size) {
                    break;
                }
                if (i == child_size + 1) {
                    return node->keys_[child_index];
                }
                i -= child_size + 1;
            }
            node = node->children_[child_index];
        }
        return std::nullopt;
    }

private:
    struct Node {
        std::size_t key_count_ = 0;
        std::array<KeyT, 2*T - 1> keys_;
        std::array<Node*, 2*T> children_{};
        std::array<std::size_t, 2*T> child_sizes_{};
    };

    static bool is_full(const Node* node) { return (node->key_count_ == 2*T - 1); }
    static bool is_leaf(const Node* node) { return !node->children_[0]; }

    static std::size_t subtree_size(const Node* node) {
        const auto sizes = node->child_sizes_.begin();
        return std::accumulate(sizes, sizes + node->key_count_ + 1, node->key_count_);
    }

    template <typename Predicate> static std::size_t count_in_node_while(const Node* node, Predicate is_before) {
        const auto keys = node->keys_.begin();
        return std::partition_point(keys, keys + node->key_count_, is_before) - keys;
    }

    std::size_t keys_before(const Node* node, const KeyT& key) const {
        return count_in_node_while(node, [this, &key](const KeyT& node_key) { return comparator_(node_key, key); });
    }

    std::size_t count_less_equal(const KeyT& key) const {
        return count_while([this, &key](const KeyT& node_key) { return !comparator_(key, node_key); });
    }

    template <typename Predicate> std::size_t count_while(Predicate is_before) const {
        std::size_t count = 0;
        const Node* node = root_;
        while (node) {
            const std::size_t keys = count_in_node_while(node, is_before);
            const auto sizes = node->child_sizes_.begin();
            count = std::accumulate(sizes, sizes + keys, count + keys);
            node = node->children_[keys];
        }
        return count;
    }

    bool contains(const KeyT& key) const {
        const Node* node = root_;
        while (node) {
            const std::size_t i = keys_before(node, key);
            if (i < node->key_count_ && !comparator_(key, node->keys_[i])) {
                return true;
            }
            node = node->children_[i];
        }
        return false;
    }

    template <typename Array> static void shift_right(Array& array, std::size_t from, std::size_t count) {
        const auto begin = array.begin();
        std::copy_backward(begin + from, begin + count, begin + count + 1);
    }

    static void insert_key(Node* node, std::size_t position, const KeyT& key) {
        shift_right(node->keys_, position, node->key_count_);
        node->keys_[position] = key;
        ++node->key_count_;
    }

    static void insert_child(Node* parent, std::size_t position, Node* child) {
        const std::size_t child_count = parent->key_count_ + 1;
        shift_right(parent->children_, position, child_count);
        shift_right(parent->child_sizes_, position, child_count);
        parent->children_[position] = child;
        parent->child_sizes_[position] = subtree_size(child);
    }

    static Node* cut_right_half(Node* left) {
        Node* right = new Node;
        std::copy(left->keys_.begin() + T, left->keys_.end(), right->keys_.begin());
        std::copy(left->children_.begin() + T, left->children_.end(), right->children_.begin());
        std::copy(left->child_sizes_.begin() + T, left->child_sizes_.end(), right->child_sizes_.begin());
        left->key_count_ = T - 1;
        right->key_count_ = T - 1;
        return right;
    }

    static void split_child(Node* parent, std::size_t child_index) {
        Node* left = parent->children_[child_index];
        Node* right = cut_right_half(left);
        insert_child(parent, child_index + 1, right);
        parent->child_sizes_[child_index] = subtree_size(left);
        insert_key(parent, child_index, left->keys_[T - 1]);
    }

    static constexpr std::size_t max_height = std::numeric_limits<std::size_t>::digits;

    static void copy_contents(const Node* source, Node* copy) {
        std::copy(source->keys_.begin(), source->keys_.begin() + source->key_count_, copy->keys_.begin());
        std::copy(source->child_sizes_.begin(), source->child_sizes_.begin() + source->key_count_ + 1, copy->child_sizes_.begin());
        copy->key_count_ = source->key_count_;
    }

    static Node* attach_child_copy(Node* parent_copy, std::size_t child_index, const Node* source_child) {
        Node* child_copy = new Node;
        parent_copy->children_[child_index] = child_copy;
        copy_contents(source_child, child_copy);
        return child_copy;
    }

    static void fill_copy(const Node* source_root, Node* copy_root) {
        struct Frame {
            const Node* source;
            Node* copy;
            std::size_t next_child;
        };
        copy_contents(source_root, copy_root);
        std::array<Frame, max_height> path;
        std::size_t depth = 0;
        path[depth++] = {source_root, copy_root, 0};
        while (depth > 0) {
            Frame& frame = path[depth - 1];
            if (is_leaf(frame.source) || frame.next_child > frame.source->key_count_) {
                --depth;
                continue;
            }
            const std::size_t child_index = frame.next_child++;
            const Node* source_child = frame.source->children_[child_index];
            Node* child_copy = attach_child_copy(frame.copy, child_index, source_child);
            path[depth++] = {source_child, child_copy, 0};
        }
    }

    static Node* copy_tree(const Node* source_root) {
        if (!source_root) {
            return nullptr;
        }
        Node* const copy_root = new Node;
        try {
            fill_copy(source_root, copy_root);
        } catch (...) {
            destroy(copy_root);
            throw;
        }
        return copy_root;
    }

    static void destroy(Node* root) {
        if (!root) {
            return;
        }
        struct Frame {
            Node* node;
            std::size_t next_child;
        };
        std::array<Frame, max_height> path;
        std::size_t depth = 0;
        path[depth++] = {root, 0};
        while (depth > 0) {
            Frame& frame = path[depth - 1];
            if (is_leaf(frame.node) || frame.next_child > frame.node->key_count_) {
                delete frame.node;
                --depth;
                continue;
            }
            Node* child = frame.node->children_[frame.next_child++];
            if (child) {
                path[depth++] = {child, 0};
            }
        }
    }

    Node* root_ = nullptr;
    Comparator comparator_;
};

} // namespace rank_queries
