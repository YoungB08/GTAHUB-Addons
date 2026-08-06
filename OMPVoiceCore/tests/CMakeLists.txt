add_executable(ov_shared_tests
    OVSharedTests.cpp
    OVChannelTests.cpp
    ../server/channels/OVChannelManager.cpp
    ../server/debug/ServerVoiceDebug.cpp)
target_link_libraries(ov_shared_tests PRIVATE ov_shared)
target_include_directories(ov_shared_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME ov_shared_tests COMMAND ov_shared_tests)

if(OV_BUILD_SERVER)
    add_executable(ov_network_integration_tests
        OVNetworkIntegrationTests.cpp
        ../client/network/OVNetworkClient.cpp
        ../server/channels/OVChannelManager.cpp
        ../server/voice/OVUdpServer.cpp
        ../server/voice/OVVoiceServer.cpp)
    target_include_directories(ov_network_integration_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(ov_network_integration_tests PRIVATE ov_shared)
    if(WIN32)
        target_link_libraries(ov_network_integration_tests PRIVATE ws2_32)
    else()
        target_link_libraries(ov_network_integration_tests PRIVATE pthread)
    endif()
    add_test(NAME ov_network_integration_tests COMMAND ov_network_integration_tests)
    set_tests_properties(ov_network_integration_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 45)
endif()
