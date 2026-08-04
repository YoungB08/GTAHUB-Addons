set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
cmake_minimum_required(VERSION 3.19)

project(HUB-ServerComponent VERSION 1.0.0 LANGUAGES CXX)

option(HUB_DEPLOY_ARTIFACTS "Copy the built component to the local open.mp server" ON)
set(HUB_OPENMP_SDK_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/openmp_sdk-src" CACHE PATH "Compatible open.mp SDK source directory")
set(HUB_OPENMP_SDK_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/openmp_sdk-src/lib" CACHE PATH "open.mp SDK third-party headers")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

FetchContent_Declare(
    openmp_sdk
    GIT_REPOSITORY https://github.com/openmultiplayer/open.mp-sdk.git
    GIT_TAG        efc11219c587dd1af791130bb937152051c05bda
)

FetchContent_Declare(
    pawn_natives
    GIT_REPOSITORY https://github.com/openmultiplayer/pawn-natives.git
    GIT_TAG        master
)

if(EXISTS "${HUB_OPENMP_SDK_DIR}/include/sdk.hpp")
    set(openmp_sdk_SOURCE_DIR "${HUB_OPENMP_SDK_DIR}")
else()
    FetchContent_MakeAvailable(openmp_sdk)
endif()

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

if(HUB_DEPLOY_ARTIFACTS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "D:/GTA-Hub/Base/components"
        COMMAND ${CMAKE_COMMAND} -E make_directory "D:/GTA-Hub/Base/qawno/include"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/pawn/hubcore_role.inc"
            "D:/GTA-Hub/Base/qawno/include/hubcore_role.inc"
        COMMAND ${CMAKE_COMMAND} -E make_directory "D:/GTA-Hub/Base/pawno/include"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/pawn/hubcore_role.inc"
            "D:/GTA-Hub/Base/pawno/include/hubcore_role.inc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "D:/GTA-Hub/Base/components/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMENT "Copying server component to D:/GTA-Hub/Base/components"
    )
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${openmp_sdk_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${HUB_OPENMP_SDK_LIB_DIR}/glm>
    $<BUILD_INTERFACE:${HUB_OPENMP_SDK_LIB_DIR}/robin-hood-hashing/src/include>
    $<BUILD_INTERFACE:${HUB_OPENMP_SDK_LIB_DIR}/span-lite/include>
    $<BUILD_INTERFACE:${HUB_OPENMP_SDK_LIB_DIR}/string-view-lite/include>
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
