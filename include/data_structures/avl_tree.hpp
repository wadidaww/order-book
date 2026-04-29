#include <algorithm>
#include <optional>

namespace ds {
template <typename T> class AVLTree {
  public:
    class Node {
      public:
        T value;
        Node *left;
        Node *right;
        int getHeight;
        int size;      // Total number of nodes in the subtree rooted at this node
        int lessThan;  // Number of nodes less than this node in the subtree rooted at this node

        Node(const T &val)
            : value(val)
            , left(nullptr)
            , right(nullptr)
            , getHeight(1)
            , size(1)
            , lessThan(0) {}
    };

    std::optional<Node> findKthNode(int k) const { return findKthNode(root, k); }

    bool insert(const T value) { return insert(root, value); }

    bool remove(const T value) { return remove(root, value); }

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