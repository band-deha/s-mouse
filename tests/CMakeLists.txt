find_package(GTest REQUIRED)

# Protocol tests
add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE smouse_core GTest::gtest GTest::gtest_main)
add_test(NAME test_protocol COMMAND test_protocol)

# Screen layout tests
add_executable(test_screen_layout test_screen_layout.cpp)
target_link_libraries(test_screen_layout PRIVATE smouse_core GTest::gtest GTest::gtest_main)
add_test(NAME test_screen_layout COMMAND test_screen_layout)

# Networking tests
add_executable(test_net test_net.cpp)
target_link_libraries(test_net PRIVATE smouse_net GTest::gtest GTest::gtest_main)
add_test(NAME test_net COMMAND test_net)
