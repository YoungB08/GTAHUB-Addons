add_executable(ov_shared_tests
    OVSharedTests.cpp
    OVChannelTests.cpp
    ../server/channels/OVChannelManager.cpp)
target_link_libraries(ov_shared_tests PRIVATE ov_shared)
target_include_directories(ov_shared_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME ov_shared_tests COMMAND ov_shared_tests)
