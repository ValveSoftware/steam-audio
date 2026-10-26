#
# Copyright 2017-2023 Valve Corporation.
#

# Toolchain for cross-compiling to Linux arm64 (aarch64) from an x64 Linux host.
#
# By default, this uses the aarch64-linux-gnu-gcc/g++ cross compilers (e.g., from the Debian/Ubuntu
# g++-aarch64-linux-gnu package), with no explicit sysroot. The following can be overridden either as CMake cache
# variables or environment variables:
#
#   STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX   Prefix for compiler and binutils names (default: aarch64-linux-gnu-)
#   STEAMAUDIO_ARM64_TOOLCHAIN_SUFFIX   Suffix for compiler names, e.g. -9 for gcc-9 (default: none)
#   STEAMAUDIO_ARM64_SYSROOT            Path to an arm64 sysroot (default: none)
#
# This file is not needed when building natively on an arm64 Linux host.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

foreach (_var STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX STEAMAUDIO_ARM64_TOOLCHAIN_SUFFIX STEAMAUDIO_ARM64_SYSROOT)
    if (NOT DEFINED ${_var} AND DEFINED ENV{${_var}})
        set(${_var} $ENV{${_var}})
    endif()
endforeach()

if (NOT DEFINED STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX)
    set(STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX aarch64-linux-gnu-)
endif()

# Toolchain files are re-evaluated in try_compile() projects, so forward our variables to them.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX
    STEAMAUDIO_ARM64_TOOLCHAIN_SUFFIX
    STEAMAUDIO_ARM64_SYSROOT
)

set(CMAKE_C_COMPILER ${STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX}gcc${STEAMAUDIO_ARM64_TOOLCHAIN_SUFFIX})
set(CMAKE_CXX_COMPILER ${STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX}g++${STEAMAUDIO_ARM64_TOOLCHAIN_SUFFIX})

if (STEAMAUDIO_ARM64_SYSROOT)
    set(CMAKE_SYSROOT ${STEAMAUDIO_ARM64_SYSROOT})
    set(CMAKE_FIND_ROOT_PATH ${STEAMAUDIO_ARM64_SYSROOT})
else()
    # Debian/Ubuntu cross toolchains install the target's C library under /usr/<triple>. Search there so that
    # find_library() finds target libraries (e.g., libm) rather than failing or finding the host's.
    string(REGEX REPLACE "-$" "" _triple "${STEAMAUDIO_ARM64_TOOLCHAIN_PREFIX}")
    if (EXISTS /usr/${_triple})
        set(CMAKE_FIND_ROOT_PATH /usr/${_triple})
    endif()
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
