//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

// OS

#if defined(_WIN32)
#define IPL_OS_WINDOWS
#elif defined(__linux__)
#if defined(__ANDROID__)
#define IPL_OS_ANDROID
#else
#define IPL_OS_LINUX
#endif
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if defined(TARGET_OS_OSX)
#define IPL_OS_MACOSX
#elif defined(TARGET_OS_IPHONE) || defined(TARGET_OS_SIMULATOR)
#define IPL_OS_IOS
#else
#pragma message "WARNING: Unsupported OS!"
#endif
#elif defined(EMSCRIPTEN) || defined(__EMSCRIPTEN__)
#define IPL_OS_WASM
#else
#pragma message "WARNING: Unsupported OS!"
#endif

// CPU architecture

#if defined(_M_IX86) || defined(__i386__)
#define IPL_CPU_X86
#elif defined(_M_X64) || defined(__x86_64__)
#define IPL_CPU_X64
#elif defined(_M_ARM) || defined(__arm__) || defined(IPL_OS_WASM)
#define IPL_CPU_ARMV7
#elif defined(_M_ARM64) || defined(__aarch64__)
#define IPL_CPU_ARM64
#else
#pragma message "WARNING: Unsupported CPU architecture!"
#endif

// Compiler

#if defined(_MSC_VER)
#define IPL_COMPILER_MSVC
#elif defined(_INTEL_COMPILER)
#define IPL_COMPILER_ICC
#elif defined(__clang__)
#define IPL_COMPILER_CLANG
#elif defined(__GNUC__)
#define IPL_COMPILER_GCC
#else
#pragma message "WARNING: Unsupported compiler!"
#endif
