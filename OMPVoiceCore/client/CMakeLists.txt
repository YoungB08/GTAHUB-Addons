set(OV_IMGUI_DIR "${imgui_SOURCE_DIR}" CACHE PATH "ImGui source directory")

if(NOT TARGET imgui)
    add_library(imgui STATIC
        "${OV_IMGUI_DIR}/imgui.cpp"
        "${OV_IMGUI_DIR}/imgui_draw.cpp"
        "${OV_IMGUI_DIR}/imgui_tables.cpp"
        "${OV_IMGUI_DIR}/imgui_widgets.cpp"
        "${OV_IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
        "${OV_IMGUI_DIR}/backends/imgui_impl_win32.cpp")
    target_include_directories(imgui PUBLIC "${OV_IMGUI_DIR}" "${OV_IMGUI_DIR}/backends")
endif()

add_library(ov_client SHARED
    OVDllMain.cpp
    audio/OVAudioEngine.cpp
    audio/OVBassApi.cpp
    audio/OVMicCapture.cpp
    audio/OVOpusEncoder.cpp
    audio/OVOpusDecoder.cpp
    audio/OVPlaybackManager.cpp
    audio/OVJitterBuffer.cpp
    audio/OVNoiseGate.cpp
    audio/OVHighPassFilter.cpp
    audio/OVAutomaticGain.cpp
    network/OVNetworkClient.cpp
    render/OVResourceManager.cpp
    render/OVHudIcon.cpp
    render/OVSpeakerRenderer.cpp
    render/OVDebugOverlay.cpp
    render/OVDx9Renderer.cpp
    ui/OVSettingsPanel.cpp
    config/OVClientConfig.cpp
    hooks/OVGameHooks.cpp
    debug/OVLoopback.cpp
    debug/OVFakeRemote.cpp
    debug/OVPacketSimulator.cpp
    debug/OVDiagnostic.cpp)

target_include_directories(ov_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${OV_IMGUI_DIR}"
    "${OV_IMGUI_DIR}/backends"
    "${minhook_SOURCE_DIR}/include")
target_link_libraries(ov_client PRIVATE ov_shared opus imgui d3d9 dxguid winmm ws2_32)
if(TARGET minhook)
    target_link_libraries(ov_client PRIVATE minhook)
endif()
target_compile_definitions(ov_client PRIVATE OMPVOICE_CLIENT_BUILD OV_CLIENT_EXPORTS)
set_target_properties(ov_client PROPERTIES OUTPUT_NAME "ov_client" PREFIX "" SUFFIX ".asi")

add_custom_command(TARGET ov_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/ompvoice"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ov_client>" "${CMAKE_BINARY_DIR}/bin/ompvoice/ov_client.asi")

if(BUILD_TESTING)
    add_executable(ov_client_audio_tests
        tests/OVAudioTests.cpp
        audio/OVOpusEncoder.cpp
        audio/OVOpusDecoder.cpp
        audio/OVJitterBuffer.cpp
        audio/OVNoiseGate.cpp
        audio/OVHighPassFilter.cpp
        audio/OVAutomaticGain.cpp)
    target_include_directories(ov_client_audio_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(ov_client_audio_tests PRIVATE opus)
    add_test(NAME ov_client_audio_tests COMMAND ov_client_audio_tests)
endif()
