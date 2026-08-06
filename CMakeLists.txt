cmake_minimum_required(VERSION 3.21)

# CMake 4.x rejects older third-party subprojects unless their policy floor is
# explicitly allowed. Keep the compatibility setting local to this build tree.
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "Minimum policy version allowed for fetched dependencies")

project(GTAHUBAddons LANGUAGES C CXX)

include(CTest)

add_subdirectory(OMPVoiceCore)
