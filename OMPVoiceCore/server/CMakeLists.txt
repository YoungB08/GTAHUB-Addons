add_library(ompvoice_server SHARED
    component/OMPVoiceCore.cpp
    channels/OVChannelManager.cpp
    voice/OVUdpServer.cpp
    voice/OVVoiceServer.cpp
    natives/OVPawnNatives.cpp
    debug/ServerVoiceDebug.cpp)

target_include_directories(ompvoice_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "$<$<BOOL:${OV_FETCH_DEPENDENCIES}>:${pawn_sdk_SOURCE_DIR}/source>")
target_link_libraries(ompvoice_server PRIVATE ov_shared OMP-SDK)
target_compile_definitions(ompvoice_server PRIVATE OMPVOICE_SERVER_BUILD HAVE_STDINT_H)
set_target_properties(ompvoice_server PROPERTIES OUTPUT_NAME "ompvoice")
if(WIN32)
    target_link_libraries(ompvoice_server PRIVATE ws2_32 dbghelp)
else()
    target_link_libraries(ompvoice_server PRIVATE pthread)
endif()

add_custom_command(TARGET ompvoice_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ompvoice_server>" "${CMAKE_BINARY_DIR}/bin/plugins/ompvoice.dll")
