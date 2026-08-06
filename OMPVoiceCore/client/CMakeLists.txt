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
    debug/OVDebugVoiceRouter.cpp
    debug/OVPacketSimulator.cpp
    debug/OVDiagnostic.cpp)

target_include_directories(ov_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${OV_IMGUI_DIR}"
    "${OV_IMGUI_DIR}/backends"
    "${minhook_SOURCE_DIR}/include"
    "${OV_BASS_ROOT}/c"
    "${OV_BASS_FX_ROOT}/C")
target_link_libraries(ov_client PRIVATE ov_shared opus imgui d3d9 dxguid winmm ws2_32 psapi windowscodecs ole32
    "${OV_BASS_ROOT}/c/bass.lib"
    "${OV_BASS_FX_ROOT}/C/bass_fx.lib")
if(TARGET minhook)
    target_link_libraries(ov_client PRIVATE minhook)
endif()
target_compile_definitions(ov_client PRIVATE OMPVOICE_CLIENT_BUILD OV_CLIENT_EXPORTS)
set_target_properties(ov_client PROPERTIES OUTPUT_NAME "ov_client" PREFIX "" SUFFIX ".asi")

add_custom_command(TARGET ov_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/ompvoice"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ov_client>" "${CMAKE_BINARY_DIR}/bin/ompvoice/ov_client.asi"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_ROOT}/bass.dll" "${CMAKE_BINARY_DIR}/bin/ompvoice/bass.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_FX_ROOT}/bass_fx.dll" "${CMAKE_BINARY_DIR}/bin/ompvoice/bass_fx.dll")

if(BUILD_TESTING)
    add_executable(ov_client_audio_tests
        tests/OVAudioTests.cpp
        debug/OVPacketSimulator.cpp
        audio/OVBassApi.cpp
        audio/OVMicCapture.cpp
        audio/OVOpusEncoder.cpp
        audio/OVOpusDecoder.cpp
        audio/OVJitterBuffer.cpp
        audio/OVNoiseGate.cpp
        audio/OVHighPassFilter.cpp
        audio/OVAutomaticGain.cpp)
    target_include_directories(ov_client_audio_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(ov_client_audio_tests PRIVATE opus ov_shared)
    add_test(NAME ov_client_audio_tests COMMAND ov_client_audio_tests)
    add_executable(ov_bass_runtime_tests tests/OVBassRuntimeTests.cpp audio/OVBassApi.cpp audio/OVMicCapture.cpp)
    target_include_directories(ov_bass_runtime_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(ov_bass_runtime_tests PRIVATE ov_shared psapi "${OV_BASS_ROOT}/c/bass.lib" "${OV_BASS_FX_ROOT}/C/bass_fx.lib")
    add_custom_command(TARGET ov_bass_runtime_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_ROOT}/bass.dll" "$<TARGET_FILE_DIR:ov_bass_runtime_tests>/bass.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_FX_ROOT}/bass_fx.dll" "$<TARGET_FILE_DIR:ov_bass_runtime_tests>/bass_fx.dll")
    add_test(NAME ov_bass_runtime_tests COMMAND ov_bass_runtime_tests)
    add_executable(ov_client_lifecycle_tests tests/OVClientLifecycleTests.cpp)
    add_dependencies(ov_client_lifecycle_tests ov_client)
    target_link_libraries(ov_client_lifecycle_tests PRIVATE psapi)
    add_test(NAME ov_client_lifecycle_tests COMMAND ov_client_lifecycle_tests "$<TARGET_FILE:ov_client>")
    set_tests_properties(ov_client_lifecycle_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 60
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/ompvoice")
    add_executable(ov_minhook_tests tests/OVMinHookTests.cpp)
    target_include_directories(ov_minhook_tests PRIVATE "${minhook_SOURCE_DIR}/include")
    target_link_libraries(ov_minhook_tests PRIVATE minhook)
    add_test(NAME ov_minhook_tests COMMAND ov_minhook_tests)
    add_executable(ov_settings_state_tests tests/OVSettingsStateTests.cpp)
    target_include_directories(ov_settings_state_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(NAME ov_settings_state_tests COMMAND ov_settings_state_tests)
    add_executable(ov_debug_modes_tests
        tests/OVDebugModesTests.cpp
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
        debug/OVDebugVoiceRouter.cpp)
    target_include_directories(ov_debug_modes_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(ov_debug_modes_tests PRIVATE opus ov_shared winmm "${OV_BASS_ROOT}/c/bass.lib" "${OV_BASS_FX_ROOT}/C/bass_fx.lib")
    add_custom_command(TARGET ov_debug_modes_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_ROOT}/bass.dll" "$<TARGET_FILE_DIR:ov_debug_modes_tests>/bass.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_FX_ROOT}/bass_fx.dll" "$<TARGET_FILE_DIR:ov_debug_modes_tests>/bass_fx.dll")
    add_test(NAME ov_debug_modes_tests COMMAND ov_debug_modes_tests)
    add_executable(ov_dx9_resource_tests
        tests/OVDx9ResourceTests.cpp
        render/OVResourceManager.cpp
        render/OVHudIcon.cpp
        render/OVSpeakerRenderer.cpp)
    target_include_directories(ov_dx9_resource_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(ov_dx9_resource_tests PRIVATE ov_shared d3d9 psapi windowscodecs ole32)
    add_test(NAME ov_dx9_resource_tests COMMAND ov_dx9_resource_tests "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    set_tests_properties(ov_dx9_resource_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 30)
    add_executable(ov_dx9_renderer_tests
        tests/OVDx9RendererTests.cpp
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
        render/OVResourceManager.cpp
        render/OVHudIcon.cpp
        render/OVSpeakerRenderer.cpp
        render/OVDebugOverlay.cpp
        render/OVDx9Renderer.cpp
        ui/OVSettingsPanel.cpp
        config/OVClientConfig.cpp
        debug/OVDiagnostic.cpp)
    target_include_directories(ov_dx9_renderer_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${OV_IMGUI_DIR}" "${OV_IMGUI_DIR}/backends")
    target_link_libraries(ov_dx9_renderer_tests PRIVATE ov_shared opus imgui d3d9 dxguid winmm windowscodecs ole32
        "${OV_BASS_ROOT}/c/bass.lib" "${OV_BASS_FX_ROOT}/C/bass_fx.lib")
    add_custom_command(TARGET ov_dx9_renderer_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_ROOT}/bass.dll" "$<TARGET_FILE_DIR:ov_dx9_renderer_tests>/bass.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_FX_ROOT}/bass_fx.dll" "$<TARGET_FILE_DIR:ov_dx9_renderer_tests>/bass_fx.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ov_dx9_renderer_tests>/ompvoice/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:ov_dx9_renderer_tests>/ompvoice/resources")
    add_test(NAME ov_dx9_renderer_tests COMMAND ov_dx9_renderer_tests)
    set_tests_properties(ov_dx9_renderer_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 30
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:ov_dx9_renderer_tests>")
endif()
