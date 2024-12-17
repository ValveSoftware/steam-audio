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

project(Phonon VERSION 4.6.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH "${CMAKE_HOME_DIRECTORY}/build")

#
# SYSTEM INTROSPECTION
#

# OS detection
if (WIN32)
    set(IPL_OS_WINDOWS TRUE)
elseif (UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(IPL_OS_LINUX TRUE)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(IPL_OS_MACOS TRUE)
elseif (ANDROID)
    set(IPL_OS_ANDROID TRUE)
elseif (APPLE AND (CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "tvOS" OR CMAKE_SYSTEM_NAME STREQUAL "visionOS"))
    set(IPL_OS_IOS TRUE)
    if (CMAKE_XCODE_ATTRIBUTE_SDKROOT STREQUAL "iphonesimulator")
        set(IPL_IOS_SIMULATOR TRUE)
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(IPL_OS_WASM TRUE)
endif()

# CPU architecture detection
if (IPL_OS_WINDOWS)
    if (CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(IPL_CPU_ARMV8 TRUE)
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(IPL_CPU_X64 TRUE)
    else()
        set(IPL_CPU_X86 TRUE)
    endif()
elseif (IPL_OS_LINUX)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(IPL_CPU_ARMV8 TRUE)
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
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


#
# OPTIONS
#

# Whether to compile a static or dynamic library
option(BUILD_SHARED_LIBS "Build shared libraries instead of static." ON)

option(STEAMAUDIO_BUILD_TESTS "Build unit tests." ON)
option(STEAMAUDIO_BUILD_BENCHMARKS "Build benchmarks." ON)
option(STEAMAUDIO_BUILD_SAMPLES "Build samples." ON)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    option(STEAMAUDIO_BUILD_ITESTS "Build interactive tests." ON)
    option(STEAMAUDIO_BUILD_DOCS "Build documentation." ON)
endif()

# If true, statically link against the C/C++ runtime library.
option(STEAMAUDIO_STATIC_RUNTIME "Statically link against the C/C++ runtime library." ON)

if ((IPL_OS_WINDOWS OR IPL_OS_LINUX) AND (IPL_CPU_X86 OR IPL_CPU_X64))
    option(STEAMAUDIO_ENABLE_AVX "Enable AVX intrinsics." ON)
endif()

if ((IPL_OS_WINDOWS AND (IPL_CPU_X86 OR IPL_CPU_X64)) OR (IPL_OS_LINUX AND (IPL_CPU_X86 OR IPL_CPU_X64)) OR IPL_OS_MACOS)
    option(STEAMAUDIO_ENABLE_IPP "Enable Intel IPP for FFT and vector math operations." ON)
    option(STEAMAUDIO_ENABLE_MKL "Enable Intel MKL support for linear algebra operations." OFF)
    option(STEAMAUDIO_ENABLE_EMBREE "Enable Intel Embree support for ray tracing." ON)
endif()

if (IPL_OS_ANDROID)
    option(STEAMAUDIO_ENABLE_FFTS "Enable FFTS for FFT operations." OFF)
endif()

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    option(STEAMAUDIO_ENABLE_RADEONRAYS "Enable AMD Radeon Rays support for GPU-accelerated ray tracing." ON)
    option(STEAMAUDIO_ENABLE_TRUEAUDIONEXT "Enable AMD TrueAudio Next support for GPU-accelerated convolution." ON)
endif()


#
# COMPILER FLAGS
#

# Flags determined by system introspection
if (IPL_OS_WINDOWS)
    add_definitions(-DIPL_OS_WINDOWS)
endif()
if (IPL_OS_LINUX)
    add_definitions(-DIPL_OS_LINUX)
endif()
if (IPL_OS_MACOS)
    add_definitions(-DIPL_OS_MACOSX)
endif()
if (IPL_OS_ANDROID)
    add_definitions(-DIPL_OS_ANDROID)
endif()
if (IPL_OS_IOS)
    add_definitions(-DIPL_OS_IOS)
endif()
if (IPL_OS_WASM)
    add_definitions(-DIPL_OS_WASM)
endif()

if (IPL_CPU_X86)
    add_definitions(-DIPL_CPU_X86)
endif()
if (IPL_CPU_X64)
    add_definitions(-DIPL_CPU_X64)
endif()
if (IPL_CPU_ARMV7)
    add_definitions(-DIPL_CPU_ARMV7)
endif()
if (IPL_CPU_ARMV8)
    add_definitions(-DIPL_CPU_ARM64)
endif()

# Windows flags
if (IPL_OS_WINDOWS)
    add_compile_definitions(
        WIN32
        WINDOWS
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _DISABLE_EXTENDED_ALIGNED_STORAGE
        $<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>
    )
    add_compile_options(
        /EHsc /MP /W3 /Zi
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${STEAMAUDIO_STATIC_RUNTIME}>>:/MTd>
        $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:${STEAMAUDIO_STATIC_RUNTIME}>>>:/MDd>
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${STEAMAUDIO_STATIC_RUNTIME}>>:/MT>
        $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${STEAMAUDIO_STATIC_RUNTIME}>>>:/MD>
        $<$<CONFIG:Debug>:/RTC1>
        $<$<NOT:$<CONFIG:Debug>>:/Ox>
        $<$<CONFIG:Release>:/GL>
    )
    add_link_options(
        /DEBUG
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/MANIFEST:NO>
        $<IF:$<CONFIG:Release>,/INCREMENTAL:NO,/INCREMENTAL>
        $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>
    )
endif()

# Linux flags
if (IPL_OS_LINUX)
    add_compile_options(-fPIC -g -fvisibility=hidden -Wl,--no-undefined)
    if (IPL_CPU_X86)
        add_compile_options(-m32 -mfpmath=sse -march=native)
    endif()
    if (STEAMAUDIO_ENABLE_AVX)
        add_compile_options(-fabi-version=6)
    endif()
endif()

# macOS flags
if (IPL_OS_MACOS)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")
    add_compile_options(-Wno-extern-c-compat) # Suppress warning about empty API structs.
    add_compile_options(-Wno-unknown-attributes) # Suppress warning in FlatBuffers.
endif()

# Android flags
if (IPL_OS_ANDROID)
    add_compile_options(-fPIC -g $<$<CONFIG:Release>:-O3>)

    # The way CMake sets up system include directories causes builds to fail
    # when including the cmath header from the standard library. This is because
    # it picks up the wrong math.h. This line forces the compiler to look in
    # the correct place first.
    include_directories(SYSTEM ${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include/c++/v1)

    add_link_options(-static-libstdc++)
endif()

# iOS flags
# todo

# WASM flags
if (IPL_OS_WASM)
    add_compile_options(-msimd128 -mfpu=neon)
    add_link_options(-sALLOW_MEMORY_GROWTH=1 -sSINGLE_FILE=1)
endif()


#
# DEPENDENCIES
#

if (STEAMAUDIO_ENABLE_IPP)
    find_package(IPP)
    if (IPP_FOUND)
        set(FFT_LIBRARY IPP)
    else()
        message(STATUS "Disabling IPP")
        set(STEAMAUDIO_ENABLE_IPP OFF)
    endif()
endif()

if (STEAMAUDIO_ENABLE_FFTS)
    find_package(FFTS)
    if (FFTS_FOUND)
        set(FFT_LIBRARY FFTS)
    else()
        message(STATUS "Disabling FFTS")
        set(STEAMAUDIO_ENABLE_FFTS OFF)
    endif()
endif()

if (NOT FFT_LIBRARY)
    find_package(PFFFT REQUIRED)
    set(FFT_LIBRARY PFFFT)
endif()

if (STEAMAUDIO_ENABLE_MKL)
    find_package(MKL)
    if (NOT MKL_FOUND)
        set(STEAMAUDIO_ENABLE_MKL OFF)
        message(STATUS "Disabling MKL")
    endif()
endif()

find_package(MySOFA REQUIRED)
find_package(FlatBuffers REQUIRED)

if (STEAMAUDIO_ENABLE_EMBREE)
    find_package(ISPC 1.12 EXACT)
    find_package(Embree 2.17 EXACT)
    if (NOT ISPC_FOUND OR NOT Embree_FOUND)
        message(STATUS "Disabling Embree")
        set(STEAMAUDIO_ENABLE_EMBREE OFF)
    endif()
endif()

if (STEAMAUDIO_ENABLE_RADEONRAYS)
    find_package(Python COMPONENTS Interpreter)
    find_package(RadeonRays)
    if (NOT Python_FOUND OR NOT RadeonRays_FOUND)
        message(STATUS "Disabling RadeonRays")
        set(STEAMAUDIO_ENABLE_RADEONRAYS OFF)
    endif()
endif()

if (STEAMAUDIO_ENABLE_TRUEAUDIONEXT)
    find_package(TrueAudioNext)
    if (NOT TrueAudioNext_FOUND)
        message(STATUS "Disabling TrueAudioNext")
        set(STEAMAUDIO_ENABLE_TRUEAUDIONEXT OFF)
    endif()
endif()

if (IPL_OS_ANDROID)
    find_package(Java 1.8 COMPONENTS Development)
endif()

if (STEAMAUDIO_BUILD_TESTS)
    find_package(Catch)
    if (NOT Catch_FOUND)
        message(STATUS "Disabling unit tests")
        set(STEAMAUDIO_BUILD_TESTS OFF)
    endif()
endif()

if (STEAMAUDIO_BUILD_ITESTS)
    find_package(GLFW)
    find_package(IMGUI)
    find_package(IMPLOT)
    find_package(PortAudio)
    find_package(DRWAV)
    if (NOT GLFW_FOUND OR NOT IMGUI_FOUND OR NOT IMPLOT_FOUND OR NOT PortAudio_FOUND OR NOT DRWAV_FOUND)
        message(STATUS "Disabling interactive tests")
        set(STEAMAUDIO_BUILD_ITESTS OFF)
    endif()
endif()

if (STEAMAUDIO_BUILD_DOCS)
    get_host_bin_subdir(IPL_BIN_SUBDIR)
    set(Doxygen_ROOT ${CMAKE_HOME_DIRECTORY}/deps/doxygen/bin/${IPL_BIN_SUBDIR})
    set(DOXYGEN_SKIP_DOT TRUE)
    find_package(Doxygen)
    find_package(Sphinx)
    if (NOT DOXYGEN_FOUND OR NOT Sphinx_FOUND)
        message(STATUS "Disabling docs")
        set(STEAMAUDIO_BUILD_DOCS OFF)
    endif()
endif()

include(CMakeListsInternal.txt OPTIONAL RESULT_VARIABLE STEAMAUDIO_INTERNAL_MODE)
if (NOT STEAMAUDIO_INTERNAL_MODE)
    set(STEAMAUDIO_PROFILING_TOOLS "None;Generic")
    set(STEAMAUDIO_PROFILING_TOOL "None" CACHE STRING "${STEAMAUDIO_PROFILING_TOOLS}")

    if(NOT STEAMAUDIO_PROFILING_TOOL IN_LIST STEAMAUDIO_PROFILING_TOOLS)
        message(FATAL_ERROR "Invalid option selected: ${STEAMAUDIO_PROFILING_TOOL}. Must be one of: ${STEAMAUDIO_PROFILING_TOOLS}.")
    endif()
endif()

# Flags determined by options
if (STEAMAUDIO_ENABLE_AVX)
    add_definitions(-DIPL_ENABLE_FLOAT8)
endif()

if (STEAMAUDIO_ENABLE_IPP)
    add_definitions(-DIPL_ENABLE_IPP)
endif()

if (STEAMAUDIO_ENABLE_FFTS)
    add_definitions(-DIPL_ENABLE_FFTS)
endif()

if (STEAMAUDIO_ENABLE_MKL)
    add_definitions(-DIPL_USE_MKL)
endif()

if (STEAMAUDIO_ENABLE_EMBREE)
    add_definitions(-DIPL_USES_EMBREE)
endif()

if (STEAMAUDIO_ENABLE_RADEONRAYS OR STEAMAUDIO_ENABLE_TRUEAUDIONEXT)
    add_definitions(-DIPL_USES_OPENCL)
endif()

if (STEAMAUDIO_ENABLE_RADEONRAYS)
    add_definitions(-DIPL_USES_RADEONRAYS)
endif()

if (STEAMAUDIO_ENABLE_TRUEAUDIONEXT)
    add_definitions(-DIPL_USES_TRUEAUDIONEXT)
endif()


#
# TARGETS
#

add_subdirectory(src/core)

if (STEAMAUDIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(src/test)
endif()

if (STEAMAUDIO_BUILD_ITESTS)
    add_subdirectory(src/itest)
endif()

if (STEAMAUDIO_BUILD_BENCHMARKS)
    add_subdirectory(src/benchmark)
endif()

if (STEAMAUDIO_BUILD_SAMPLES)
    add_subdirectory(src/samples)
endif()

if (STEAMAUDIO_BUILD_DOCS)
    add_subdirectory(doc)
endif()


#
# PACKAGING
#

install(FILES ${CMAKE_HOME_DIRECTORY}/THIRDPARTY.md DESTINATION root)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/git/README.md.in ${CMAKE_CURRENT_BINARY_DIR}/git/README.md)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/git/downloads.html.in ${CMAKE_CURRENT_BINARY_DIR}/git/downloads.html)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/git/README.md DESTINATION git)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/git/downloads.html DESTINATION git)

set(CPACK_PACKAGE_VENDOR "Valve Corporation")
set(CPACK_PACKAGE_FILE_NAME steamaudio)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_HOME_DIRECTORY}/dist)
set(CPACK_INSTALL_CMAKE_PROJECTS "")
set(CPACK_INSTALLED_DIRECTORIES
    ${CMAKE_INSTALL_PREFIX}/include include
    ${CMAKE_INSTALL_PREFIX}/lib/windows-x86 lib/windows-x86
    ${CMAKE_INSTALL_PREFIX}/lib/windows-x64 lib/windows-x64
    ${CMAKE_INSTALL_PREFIX}/lib/linux-x86 lib/linux-x86
    ${CMAKE_INSTALL_PREFIX}/lib/linux-x64 lib/linux-x64
    ${CMAKE_INSTALL_PREFIX}/lib/osx lib/osx
    ${CMAKE_INSTALL_PREFIX}/lib/android-armv7 lib/android-armv7
    ${CMAKE_INSTALL_PREFIX}/lib/android-armv8 lib/android-armv8
    ${CMAKE_INSTALL_PREFIX}/lib/android-x86 lib/android-x86
    ${CMAKE_INSTALL_PREFIX}/lib/android-x64 lib/android-x64
    ${CMAKE_INSTALL_PREFIX}/lib/ios lib/ios
    ${CMAKE_INSTALL_PREFIX}/lib/wasm lib/wasm
    ${CMAKE_INSTALL_PREFIX}/doc doc
    ${CMAKE_INSTALL_PREFIX}/symbols/windows-x86 symbols/windows-x86
    ${CMAKE_INSTALL_PREFIX}/symbols/windows-x64 symbols/windows-x64
    ${CMAKE_INSTALL_PREFIX}/symbols/linux-x86 symbols/linux-x86
    ${CMAKE_INSTALL_PREFIX}/symbols/linux-x64 symbols/linux-x64
    ${CMAKE_INSTALL_PREFIX}/root .
)

include(CPack)
