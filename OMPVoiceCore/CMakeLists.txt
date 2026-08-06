cmake_minimum_required(VERSION 3.21)

project(OMPVoiceCore VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
include(CTest)

option(OV_BUILD_SERVER "Build the open.mp server component" ON)
option(OV_BUILD_CLIENT "Build the SA:MP 0.3.DL R1 ASI client" ON)
option(OV_BUILD_TESTS "Build OMPVoiceCore unit tests" ON)
option(OV_FETCH_DEPENDENCIES "Download open-source dependencies at configure time" ON)

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
        add_subdirectory(client)
    else()
        message(STATUS "ov_client is Win32-only; the server and tests remain available on this platform")
    endif()
endif()

if(BUILD_TESTING AND OV_BUILD_TESTS)
    add_subdirectory(tests)
endif()
