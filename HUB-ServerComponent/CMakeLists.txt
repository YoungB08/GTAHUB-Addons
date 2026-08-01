set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
cmake_minimum_required(VERSION 3.19)

project(HUB-ServerComponent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

FetchContent_Declare(
    openmp_sdk
    GIT_REPOSITORY https://github.com/openmultiplayer/open.mp-sdk.git
    GIT_TAG        master
)

FetchContent_Declare(
    pawn_natives
    GIT_REPOSITORY https://github.com/openmultiplayer/pawn-natives.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(openmp_sdk)

FetchContent_GetProperties(pawn_natives)
if(NOT pawn_natives_POPULATED)
    FetchContent_Populate(pawn_natives)
endif()

# Source files
set(SOURCES
    src/RoleComponent.cpp
    src/ScriptNatives.cpp
)

set(HEADERS
    include/RoleComponent.hpp
    include/PlayerRoleData.hpp
    include/Logger.hpp
    include/amx/amx.h
)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${openmp_sdk_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${openmp_sdk_SOURCE_DIR}/lib/glm>
    $<BUILD_INTERFACE:${openmp_sdk_SOURCE_DIR}/lib/robin-hood-hashing/src/include>
    $<BUILD_INTERFACE:${openmp_sdk_SOURCE_DIR}/lib/span-lite/include>
    $<BUILD_INTERFACE:${openmp_sdk_SOURCE_DIR}/lib/string-view-lite/include>
    $<BUILD_INTERFACE:${pawn_natives_SOURCE_DIR}>
    $<BUILD_INTERFACE:${pawn_natives_SOURCE_DIR}/include>
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /MP)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation target
install(TARGETS ${PROJECT_NAME} DESTINATION components)
