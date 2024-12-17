# Copyright 2017-2023 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.17)

project(SteamAudioWwise VERSION 4.6.0)
set(CMAKE_MODULE_PATH ${CMAKE_HOME_DIRECTORY}/build)


# ---------------------------------------------------------------------------------------------------------------------
# SYSTEM INTROSPECTION
# ---------------------------------------------------------------------------------------------------------------------

# OS detection
if (WIN32)
    set(IPL_OS_WINDOWS TRUE)
elseif (UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(IPL_OS_LINUX TRUE)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(IPL_OS_MACOS TRUE)
elseif (ANDROID)
    set(IPL_OS_ANDROID TRUE)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IPL_OS_IOS TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(IPL_OS_WASM TRUE)
endif()

# CPU architecture detection
if (IPL_OS_WINDOWS OR IPL_OS_LINUX)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(IPL_CPU_X64 TRUE)
    else()
        set(IPL_CPU_X86 TRUE)
    endif()
elseif (IPL_OS_MACOS)
elseif (IPL_OS_ANDROID)
    if (CMAKE_ANDROID_ARCH STREQUAL "arm")
        set(IPL_CPU_ARMV7 TRUE)
    elseif (CMAKE_ANDROID_ARCH STREQUAL "arm64")
        set(IPL_CPU_ARMV8 TRUE)
    elseif (CMAKE_ANDROID_ARCH STREQUAL "x86")
        set(IPL_CPU_X86 TRUE)
    elseif (CMAKE_ANDROID_ARCH STREQUAL "x86_64")
        set(IPL_CPU_X64 TRUE)
    endif()
elseif (IPL_OS_IOS)
    set(IPL_CPU_ARMV8 TRUE)
elseif (IPL_OS_WASM)
    set(IPL_CPU_ARMV7 TRUE)
endif()


# ---------------------------------------------------------------------------------------------------------------------
# OPTIONS
# ---------------------------------------------------------------------------------------------------------------------

option(STEAMAUDIOWWISE_BUILD_DOCS "Build documentation." OFF)


# ---------------------------------------------------------------------------------------------------------------------
# TARGETS
# ---------------------------------------------------------------------------------------------------------------------

add_subdirectory(src)

if (STEAMAUDIOWWISE_BUILD_DOCS)
    add_subdirectory(doc)
endif()

install(FILES ${CMAKE_HOME_DIRECTORY}/../core/THIRDPARTY.md DESTINATION root)


# ---------------------------------------------------------------------------------------------------------------------
# PACKAGING
# ---------------------------------------------------------------------------------------------------------------------

set(CPACK_PACKAGE_VENDOR "Valve Corporation")
set(CPACK_PACKAGE_FILE_NAME steamaudio_wwise)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_HOME_DIRECTORY}/dist)
set(CPACK_INSTALL_CMAKE_PROJECTS "")
set(CPACK_EXTERNAL_PACKAGE_SCRIPT ${CMAKE_HOME_DIRECTORY}/build/package_wwise.cmake)
set(CPACK_INSTALLED_DIRECTORIES
    ${CMAKE_INSTALL_PREFIX}/wwise_launcher wwise
    ${CMAKE_INSTALL_PREFIX}/doc doc
    ${CMAKE_INSTALL_PREFIX}/root .
)

include(CPack)
