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

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <algorithm>
#include <atomic>
#include <complex>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <forward_list>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#endif

#include "platform.h"

#if (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
#include <emmintrin.h>
#include <xmmintrin.h>
#if defined(IPL_ENABLE_FLOAT8)
#include <immintrin.h>
#endif
#endif

#if (defined(IPL_CPU_ARMV7) || defined(IPL_CPU_ARM64))
#include <arm_neon.h>
#endif

#if defined(IPL_OS_WINDOWS)
#ifdef __cplusplus
#include <codecvt>
#endif
#include <Windows.h>
#include <delayimp.h>
#endif

#if defined(IPL_OS_MACOSX)
#include <mach-o/dyld.h>
#endif

#if (defined(IPL_OS_LINUX) || defined(IPL_OS_MACOSX) || defined(IPL_OS_ANDROID))
#include <dlfcn.h>
#endif

#if defined(IPL_OS_ANDROID)
#include <android/log.h>
#endif

#if (defined(IPL_ENABLE_IPP) && (defined(IPL_OS_WINDOWS) || (defined(IPL_OS_LINUX) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))) || defined(IPL_OS_MACOSX)))
#include <ipp.h>
#include <ipps.h>
#endif

#if defined(IPL_USE_MKL)
#include <mkl_blas.h>
#include <mkl_cblas.h>
#include <mkl_lapack.h>
#include <mkl_lapacke.h>
#include <mkl_service.h>
#endif

#if (defined(IPL_ENABLE_FFTS) && defined(IPL_OS_ANDROID))
#include <ffts.h>
#endif

#include <mysofa.h>

#ifdef __cplusplus
#include <flatbuffers/flatbuffers.h>
#endif

#ifdef __cplusplus
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
#define EMBREE_STATIC_LIB
#include <rtcore.h>
#include <rtcore_ray.h>
#endif
#endif

#if defined(IPL_USES_OPENCL)
#include <CL/cl.h>
#endif

#ifdef __cplusplus
#if defined(IPL_USES_RADEONRAYS)
#define USE_OPENCL 1
#define RR_STATIC_LIBRARY 1
#include <radeon_rays_cl.h>
#endif
#endif

#ifdef __cplusplus
#if defined(IPL_USES_TRUEAUDIONEXT)
#include <TrueAudioNext.h>
#include <public/include/core/Factory.h>
#include <GpuUtilities.h>
#endif
#endif
