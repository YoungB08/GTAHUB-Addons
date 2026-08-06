add_executable(ov_shared_tests
    OVSharedTests.cpp
    OVChannelTests.cpp
    ../server/channels/OVChannelManager.cpp
    ../server/debug/ServerVoiceDebug.cpp)
target_link_libraries(ov_shared_tests PRIVATE ov_shared)
target_include_directories(ov_shared_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME ov_shared_tests COMMAND ov_shared_tests)

if(OV_BUILD_SERVER)
    add_executable(ov_native_lookup_tests
        OVNativeLookupTests.cpp
        ../server/natives/OVPawnNatives.cpp
        ../server/channels/OVChannelManager.cpp)
    target_include_directories(ov_native_lookup_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "$<$<BOOL:${OV_FETCH_DEPENDENCIES}>:${pawn_sdk_SOURCE_DIR}/source>")
    target_compile_definitions(ov_native_lookup_tests PRIVATE HAVE_STDINT_H)
    target_link_libraries(ov_native_lookup_tests PRIVATE ov_shared OMP-SDK)
    add_test(NAME ov_native_lookup_tests COMMAND ov_native_lookup_tests)

    if(WIN32)
        add_executable(ov_component_lifecycle_tests OVComponentLifecycleTests.cpp)
        target_link_libraries(ov_component_lifecycle_tests PRIVATE OMP-SDK)
        add_dependencies(ov_component_lifecycle_tests ompvoice_server)
        add_test(NAME ov_component_lifecycle_tests COMMAND ov_component_lifecycle_tests "$<TARGET_FILE:ompvoice_server>")
    endif()

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
    if(TARGET pawncc)
        add_test(NAME pawn_gamemode_compile
            COMMAND $<TARGET_FILE:pawncc>
                "-i${CMAKE_CURRENT_SOURCE_DIR}/../pawno/include"
                "-o${CMAKE_CURRENT_BINARY_DIR}/voice_smoke.amx"
                "${CMAKE_CURRENT_SOURCE_DIR}/../examples/voice_smoke.pwn")
        set_tests_properties(pawn_gamemode_compile PROPERTIES RUN_SERIAL TRUE TIMEOUT 30)
    endif()
endif()
