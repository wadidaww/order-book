#pragma once

#include <algorithm>
#include <optional>
#include <memory>
#include <boost/pool/object_pool.hpp>

namespace ds {

// AVL tree with O(log n) insert / remove / k-th element queries.
//
// Node allocation is handled by boost::object_pool, which:
//   • Pre-allocates nodes in large chunks, eliminating per-node heap overhead.
//   • Returns nodes to the pool for reuse (O(1) dealloc) rather than calling
//     the global operator delete each time.
//   • Frees all pooled memory in a single step when the pool is destroyed or
//     reset (e.g., on clear()).
//
// The pool is owned through std::unique_ptr so that clear() can replace it
// with a fresh instance — triggering the old pool's destructor (which destroys
// all still-allocated nodes and releases the backing memory) — without any
// manual destructor/placement-new gymnastics.
template <typename T> class AVLTree {
  public:
    class Node {
      public:
        T value;
        Node *left;
        Node *right;
        int getHeight;  // height of the subtree rooted at this node (leaf = 1)
        int size;       // Total number of nodes in the subtree rooted at this node
        int lessThan;   // Number of nodes less than this node in the subtree rooted at this node

        Node(const T &val)
            : value(val)
            , left(nullptr)
            , right(nullptr)
            , getHeight(1)
            , size(1)
            , lessThan(0) {}
    };

    std::optional<Node> findKthNode(int k) const { return findKthNode(root_, k); }

    bool insert(const T value) { return insert(root_, value); }

    bool remove(const T value) { return remove(root_, value); }

    // Reset the tree: drop all nodes and release all pool memory in one shot.
    // Replacing the unique_ptr triggers the pool destructor, which destroys
    // every outstanding Node and frees the backing chunks — O(1) for the
    // memory-release itself regardless of tree size.
    void clear() {
        root_ = nullptr;
        pool_ = std::make_unique<Pool>();
    }

    AVLTree()
        : root_(nullptr)
        , pool_(std::make_unique<Pool>()) {}

    // pool_ unique_ptr destructor calls ~object_pool(), which destroys all
    // remaining nodes and returns pool chunks to the OS automatically.
    ~AVLTree() = default;

    // Non-copyable (pool is a unique resource)
    AVLTree(const AVLTree &) = delete;
    AVLTree &operator=(const AVLTree &) = delete;

  private:
    using Pool = boost::object_pool<Node>;

    Node *root_;
    std::unique_ptr<Pool> pool_;

    inline std::optional<Node> findKthNode(Node *node, int k) const {
        if (!node || k < 1 || k > node->size)
            return std::nullopt;
        int leftSize = node->left ? node->left->size : 0;
        if (k <= leftSize) {
            return findKthNode(node->left, k);
        } else if (k == leftSize + 1) {
            return *node;
        } else {
            return findKthNode(node->right, k - leftSize - 1);
        }
    }

    Node *minValueNode(Node *node) const {
        Node *current = node;
        while (current && current->left)
            current = current->left;
        return current;
    }

    inline bool insert(Node *&node, const T &value) {
        if (!node) {
            // Allocate from the pool — O(1) amortised, no global heap call
            node = pool_->construct(value);
            return true;
        }
        if (value < node->value) {
            if (!insert(node->left, value))
                return false;
        } else if (value > node->value) {
            if (!insert(node->right, value))
                return false;
        } else {
            return false;
        }
        rebalance(node);
        return true;
    }

    inline bool remove(Node *&node, const T &value) {
        if (!node)
            return false;
        if (value < node->value) {
            if (!remove(node->left, value))
                return false;
        } else if (value > node->value) {
            if (!remove(node->right, value))
                return false;
        } else {
            // Node with only 0-1 child
            if (!node->left || !node->right) {
                Node *temp = node->left ? node->left : node->right;
                // Return node to the pool — O(1), no global heap call
                pool_->destroy(node);
                node = temp;
            } else {
                // Node with two children: get the inorder successor
                Node *temp = minValueNode(node->right);
                node->value = temp->value;
                remove(node->right, temp->value);  // Delete the inorder successor
            }
        }
        rebalance(node);
        return true;
    }

    void rebalance(Node *&node) {
        if (!node)
            return;
        node->getHeight = 1 + std::max(getHeight(node->left), getHeight(node->right));
        node->size = 1 + getSize(node->left) + getSize(node->right);
        node->lessThan = getSize(node->left);
        int balanceFactor = getBalanceFactor(node);

        // Left Left Case
        if (balanceFactor > 1 && getBalanceFactor(node->left) >= 0) {
            rotateRight(node);
            return;
        }

        // Left Right Case
        if (balanceFactor > 1 && getBalanceFactor(node->left) < 0) {
            rotateLeft(node->left);
            rotateRight(node);
            return;
        }

        // Right Right Case
        if (balanceFactor < -1 && getBalanceFactor(node->right) <= 0) {
            rotateLeft(node);
            return;
        }

        // Right Left Case
        if (balanceFactor < -1 && getBalanceFactor(node->right) > 0) {
            rotateRight(node->right);
            rotateLeft(node);
            return;
        }
    }

    int getHeight(Node *node) const {
        if (!node)
            return 0;
        return node->getHeight;
    }

    int getSize(Node *node) const {
        if (!node)
            return 0;
        return node->size;
    }

    int getBalanceFactor(Node *node) const {
        if (!node)
            return 0;
        return getHeight(node->left) - getHeight(node->right);
    }

    void rotateLeft(Node *&node) {
        if (!node || !node->right)
            return;
        Node *newRoot = node->right;
        node->right = newRoot->left;
        newRoot->left = node;
        node = newRoot;
    }

    void rotateRight(Node *&node) {
        if (!node || !node->left)
            return;
        Node *newRoot = node->left;
        node->left = newRoot->right;
        newRoot->right = node;
        node = newRoot;
    }
};

}  // namespace ds