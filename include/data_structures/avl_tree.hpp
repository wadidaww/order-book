#pragma once

#include <algorithm>
#include <optional>

namespace ds {

// A self-balancing AVL binary search tree augmented with order-statistic
// metadata.  Every node stores:
//   • size     — total number of nodes in its subtree (used for rank queries)
//   • lessThan — number of nodes in its subtree that are strictly less than
//                the node's own value (equal to the left-subtree size)
//
// Supported operations (all O(log n)):
//   insert(value)      — insert a new value; no-op if already present
//   remove(value)      — remove a value; no-op if not found
//   findKthNode(k)     — return the k-th smallest element (1-indexed)
//   clear()            — delete all nodes and reset the tree
//
// T must be totally ordered via operator< and operator>.
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

    // Return the k-th smallest node (1-indexed) in the tree, or std::nullopt
    // if k is out of range.  O(log n).
    std::optional<Node> findKthNode(int k) const { return findKthNode(root, k); }

    // Insert value into the tree.  Returns true on success, false if value is
    // already present.  O(log n).
    bool insert(const T value) { return insert(root, value); }

    // Remove value from the tree.  Returns true on success, false if value was
    // not found.  O(log n).
    bool remove(const T value) { return remove(root, value); }

    // Delete all nodes and reset the tree to empty.  O(n).
    void clear() {
        clear(root);
        root = nullptr;
    }

    AVLTree()
        : root(nullptr) {}
    ~AVLTree() { clear(root); }

  private:
    Node *root;

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
            node = new Node(value);
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
                delete node;
                node = temp;
            } else {
                // Node with two children: Get the inorder successor
                Node *temp = minValueNode(node->right);
                node->value = temp->value;
                remove(node->right, temp->value);  // Delete the inorder successor
            }
        }
        rebalance(node);
        return true;
    }

    void clear(Node *node) {
        if (!node)
            return;
        clear(node->left);
        clear(node->right);
        delete node;
    }

    // Update height/size/lessThan for node, then rotate if |balance| > 1.
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