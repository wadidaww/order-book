# Create separate test executables
add_executable(test_avl_tree test_avl_tree.cpp)
target_link_libraries(test_avl_tree PRIVATE orderbook)

# Add tests
add_test(NAME AVLTreeTests COMMAND test_avl_tree)
