#
# Copyright 2017-2023 Valve Corporation.
#

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_FIND_ROOT_PATH /sysroot)
set(CMAKE_SYSROOT /sysroot)

set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc-9)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++-9)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
