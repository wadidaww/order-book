#include <data_structures.hpp>
#include "../test_framework.hpp"

using namespace ds;
using namespace test;

TEST(avl_tree_kth_node) {
    AVLTree<int> tree;

    tree.insert(10);
    tree.insert(20);
    tree.insert(5);
    tree.insert(15);
    tree.insert(25);

    // duplicate insertions should be ignored
    ASSERT_FALSE(tree.insert(10));
    ASSERT_FALSE(tree.insert(20));

    // find kth nodes
    auto node = tree.findKthNode(3);
    ASSERT_TRUE(node.has_value());
    ASSERT_EQ(node->value, 15);

    node = tree.findKthNode(1);
    ASSERT_TRUE(node.has_value());
    ASSERT_EQ(node->value, 5);

    node = tree.findKthNode(5);
    ASSERT_TRUE(node.has_value());
    ASSERT_EQ(node->value, 25);

    node = tree.findKthNode(6);
    ASSERT_FALSE(node.has_value());

    // some removals
    tree.remove(10);
    node = tree.findKthNode(3);
    ASSERT_TRUE(node.has_value());
    ASSERT_EQ(node->value, 20);

    tree.remove(5);
    node = tree.findKthNode(1);
    ASSERT_TRUE(node.has_value());
    ASSERT_EQ(node->value, 15);

    tree.remove(25);
    node = tree.findKthNode(3);
    ASSERT_FALSE(node.has_value());

    // check clear
    tree.clear();
    node = tree.findKthNode(1);
    ASSERT_FALSE(node.has_value());
}

int main() {
    return TestSuite::instance().run();
}