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
    add_subdirectory(server)
endif()

if(OV_BUILD_CLIENT)
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_subdirectory(client)
    else()
        message(STATUS "ov_client is Win32-only; the server and tests remain available on this platform")
    endif()
endif()

if(BUILD_TESTING AND OV_BUILD_TESTS)
    add_subdirectory(tests)
endif()
