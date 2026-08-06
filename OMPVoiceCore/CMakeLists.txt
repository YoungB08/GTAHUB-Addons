cmake_minimum_required(VERSION 3.21)

project(OMPVoiceCore VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
include(CTest)

option(OV_BUILD_SERVER "Build the open.mp server component" ON)
option(OV_BUILD_CLIENT "Build the SA:MP 0.3.DL R1 ASI client" ON)
option(OV_BUILD_TESTS "Build OMPVoiceCore unit tests" ON)
option(OV_FETCH_DEPENDENCIES "Download open-source dependencies at configure time" ON)
set(OV_BASS_ROOT "" CACHE PATH "Official BASS 2.4 Win32 package root (contains c/bass.lib and bass.dll)")
set(OV_BASS_FX_ROOT "" CACHE PATH "Official BASS FX 2.4 Win32 package root (contains C/bass_fx.lib and bass_fx.dll)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(OV_FETCH_DEPENDENCIES)
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(nlohmann_json)
else()
    find_package(nlohmann_json 3.11 REQUIRED)
endif()

add_library(ov_shared STATIC
    shared/OVConfig.cpp
    shared/OVCrashSafety.cpp
    shared/OVLogger.cpp
    shared/OVPacket.cpp)
target_include_directories(ov_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ov_shared PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(ov_shared PUBLIC cxx_std_17)

if(OV_BUILD_SERVER)
    if(OV_FETCH_DEPENDENCIES)
        FetchContent_Declare(
            omp_sdk
            GIT_REPOSITORY https://github.com/openmultiplayer/open.mp-sdk.git
            GIT_TAG 3ee7bc4ab20c22359c34c08c38f93815b44bffd5
            GIT_SHALLOW FALSE
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(omp_sdk)
    else()
        find_package(OMP-SDK CONFIG REQUIRED)
    endif()
    if(OV_FETCH_DEPENDENCIES)
        FetchContent_Declare(
            pawn_sdk
            GIT_REPOSITORY https://github.com/pawn-lang/compiler.git
            GIT_TAG 1d13e22842bb54270c69696e7980aae7f9ae6a1f
            GIT_SHALLOW FALSE)
        FetchContent_GetProperties(pawn_sdk)
        if(NOT pawn_sdk_POPULATED)
            FetchContent_Populate(pawn_sdk)
        endif()
        if(EXISTS "${pawn_sdk_SOURCE_DIR}/source/compiler/CMakeLists.txt" AND NOT TARGET pawncc)
            set(PAWN_BUILD_TESTS_SAVED "${BUILD_TESTING}")
            set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
            add_subdirectory("${pawn_sdk_SOURCE_DIR}/source/compiler" "${CMAKE_BINARY_DIR}/pawn_compiler")
            set(BUILD_TESTING "${PAWN_BUILD_TESTS_SAVED}" CACHE BOOL "" FORCE)
        endif()
    endif()
    add_subdirectory(server)
endif()

if(OV_BUILD_CLIENT)
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        if(OV_FETCH_DEPENDENCIES)
            FetchContent_Declare(
                opus
                GIT_REPOSITORY https://github.com/xiph/opus.git
                GIT_TAG v1.5.2
                GIT_SHALLOW TRUE)
            set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
            set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
            set(OV_BUILD_TESTING_SAVED "${BUILD_TESTING}")
            set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(opus)
            set(BUILD_TESTING "${OV_BUILD_TESTING_SAVED}" CACHE BOOL "" FORCE)

            FetchContent_Declare(
                imgui
                GIT_REPOSITORY https://github.com/ocornut/imgui.git
                GIT_TAG v1.91.9b
                GIT_SHALLOW TRUE)
            FetchContent_GetProperties(imgui)
            if(NOT imgui_POPULATED)
                FetchContent_Populate(imgui)
            endif()

            FetchContent_Declare(
                minhook
                GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
                GIT_TAG v1.3.4
                GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(minhook)
        endif()

        # BASS is proprietary, so the repository contains no binary. Fetch the
        # official archives by checksum at configure time and use their Win32
        # import libraries/headers; the resulting package remains subject to
        # the BASS license and is not committed.
        if(NOT OV_BASS_ROOT OR NOT OV_BASS_FX_ROOT)
            if(NOT OV_FETCH_DEPENDENCIES)
                message(FATAL_ERROR "Win32 client requires OV_BASS_ROOT and OV_BASS_FX_ROOT when OV_FETCH_DEPENDENCIES=OFF")
            endif()
            FetchContent_Declare(
                ov_bass_runtime
                URL https://www.un4seen.com/files/bass24.zip
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                URL_HASH SHA256=3A03EC9A33D0F4F9D167660DA51C8BB1432E8977496995455AB137277D69636E)
            FetchContent_Declare(
                ov_bass_fx_runtime
                URL https://www.un4seen.com/files/z/0/bass_fx24.zip
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                URL_HASH SHA256=A4BAF602865941963127ACB15ED12627D108189F99C2757970432AE7DA0366CD)
            FetchContent_MakeAvailable(ov_bass_runtime ov_bass_fx_runtime)
            set(OV_BASS_ROOT "${ov_bass_runtime_SOURCE_DIR}" CACHE PATH "Official BASS 2.4 Win32 package root" FORCE)
            set(OV_BASS_FX_ROOT "${ov_bass_fx_runtime_SOURCE_DIR}" CACHE PATH "Official BASS FX 2.4 Win32 package root" FORCE)
        endif()
        if(NOT EXISTS "${OV_BASS_ROOT}/c/bass.lib" OR NOT EXISTS "${OV_BASS_ROOT}/bass.dll")
            message(FATAL_ERROR "OV_BASS_ROOT must contain c/bass.lib and bass.dll")
        endif()
        if(NOT EXISTS "${OV_BASS_FX_ROOT}/C/bass_fx.lib" OR NOT EXISTS "${OV_BASS_FX_ROOT}/bass_fx.dll")
            message(FATAL_ERROR "OV_BASS_FX_ROOT must contain C/bass_fx.lib and bass_fx.dll")
        endif()
        set(OV_BASS_RUNTIME_DIR "${OV_BASS_ROOT}" CACHE PATH "Resolved BASS runtime directory" FORCE)
        set(OV_BASS_FX_RUNTIME_DIR "${OV_BASS_FX_ROOT}" CACHE PATH "Resolved BASS FX runtime directory" FORCE)
        add_subdirectory(client)
    else()
        message(STATUS "ov_client is Win32-only; the server and tests remain available on this platform")
    endif()
endif()

if(BUILD_TESTING AND OV_BUILD_TESTS)
    add_subdirectory(tests)
endif()

set(OV_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
set(OV_PACKAGE_DEPENDS)
if(TARGET ompvoice_server)
    list(APPEND OV_PACKAGE_DEPENDS ompvoice_server)
endif()
if(TARGET ov_client)
    list(APPEND OV_PACKAGE_DEPENDS ov_client)
endif()
set(OV_PACKAGE_ARTIFACT_COMMANDS)
if(TARGET ompvoice_server)
    list(APPEND OV_PACKAGE_ARTIFACT_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:ompvoice_server>" "${OV_PACKAGE_DIR}/plugins/ompvoice.dll")
endif()
if(TARGET ov_client)
    list(APPEND OV_PACKAGE_ARTIFACT_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:ov_client>" "${OV_PACKAGE_DIR}/ompvoice/ov_client.asi")
    list(APPEND OV_PACKAGE_ARTIFACT_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_ROOT}/bass.dll" "${OV_PACKAGE_DIR}/ompvoice/bass.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_FX_ROOT}/bass_fx.dll" "${OV_PACKAGE_DIR}/ompvoice/bass_fx.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_ROOT}/bass.txt" "${OV_PACKAGE_DIR}/licenses/BASS.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OV_BASS_FX_ROOT}/bass_fx.txt" "${OV_PACKAGE_DIR}/licenses/BASS_FX.txt")
endif()
add_custom_target(ompvoice_package ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OV_PACKAGE_DIR}/plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OV_PACKAGE_DIR}/ompvoice/resources"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OV_PACKAGE_DIR}/ompvoice/logs"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OV_PACKAGE_DIR}/ompvoice/records"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OV_PACKAGE_DIR}/ompvoice/debug"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OV_PACKAGE_DIR}/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/config/config.json" "${OV_PACKAGE_DIR}/ompvoice/config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/client/resources" "${OV_PACKAGE_DIR}/ompvoice/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/pawno/include/ompvoice.inc" "${OV_PACKAGE_DIR}/ompvoice/ompvoice.inc"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/README.md" "${OV_PACKAGE_DIR}/README.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/INSTALL.md" "${OV_PACKAGE_DIR}/INSTALL.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md" "${OV_PACKAGE_DIR}/CHANGELOG.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "${OV_PACKAGE_DIR}/licenses/OMPVoiceCore-LICENSE.txt"
    ${OV_PACKAGE_ARTIFACT_COMMANDS}
    DEPENDS ${OV_PACKAGE_DEPENDS})
