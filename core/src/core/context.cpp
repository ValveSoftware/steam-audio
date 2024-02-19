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

#include "context.h"

#include "platform.h"

#if defined(IPL_OS_WINDOWS) || defined(IPL_OS_MACOSX)
#if defined(IPL_USE_MKL)
#include <mkl_service.h>
#endif
#endif

#if defined(IPL_ENABLE_IPP) && (defined(IPL_OS_WINDOWS) || defined(IPL_OS_LINUX) || (defined(IPL_OS_MACOSX) && defined(IPL_CPU_X64)))
#include <ipp.h>
#endif

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Context
// --------------------------------------------------------------------------------------------------------------------

Log Context::sLog;
Memory Context::sMemory;
SIMDLevel Context::sSIMDLevel = SIMDLevel::SSE2;
uint32_t Context::sAPIVersion = 0;

Context::Context(LogCallback logCallback,
                 AllocateCallback allocateCallback,
                 FreeCallback freeCallback,
                 SIMDLevel simdLevel,
                 uint32_t apiVersion)
{
    sAPIVersion = apiVersion;

    sLog.init(logCallback);

    sMemory.init(allocateCallback, freeCallback);

#if defined(IPL_ENABLE_IPP) && (defined(IPL_OS_WINDOWS) || defined(IPL_OS_LINUX) || (defined(IPL_OS_MACOSX) && defined(IPL_CPU_X64)))
    Ipp64u cpuFeatures = 0;
    ippGetCpuFeatures(&cpuFeatures, nullptr);

    SIMDLevel supportedSIMDLevel = SIMDLevel::SSE2;
    if (cpuFeatures & ippCPUID_AVX512F)
    {
        supportedSIMDLevel = SIMDLevel::AVX512;
    }
    else if (cpuFeatures & ippCPUID_AVX2)
    {
        supportedSIMDLevel = SIMDLevel::AVX2;
    }
    else if (cpuFeatures & ippCPUID_AVX)
    {
        supportedSIMDLevel = SIMDLevel::AVX;
    }
    else if (cpuFeatures & ippCPUID_SSE42)
    {
        supportedSIMDLevel = SIMDLevel::SSE4;
    }
    else
    {
        supportedSIMDLevel = SIMDLevel::SSE2;
    }

    sSIMDLevel = std::min(simdLevel, supportedSIMDLevel);

    cpuFeatures = ippCPUID_MMX | ippCPUID_SSE | ippCPUID_SSE2;
    if (sSIMDLevel >= SIMDLevel::SSE4)
        cpuFeatures = cpuFeatures | ippCPUID_SSE3 | ippCPUID_SSSE3 | ippCPUID_SSE41 | ippCPUID_SSE42 | ippCPUID_AES | ippCPUID_CLMUL | ippCPUID_SHA;
    if (sSIMDLevel >= SIMDLevel::AVX)
        cpuFeatures = cpuFeatures | ippCPUID_AVX | ippAVX_ENABLEDBYOS | ippCPUID_RDRAND | ippCPUID_F16C;
    if (sSIMDLevel >= SIMDLevel::AVX2)
        cpuFeatures = cpuFeatures | ippCPUID_AVX2 | ippCPUID_MOVBE | ippCPUID_ADCOX | ippCPUID_RDSEED | ippCPUID_PREFETCHW;
#if defined(IPL_CPU_X64)
    if (sSIMDLevel >= SIMDLevel::AVX512)
        cpuFeatures = cpuFeatures | ippCPUID_AVX512F;
#endif

    ippSetCpuFeatures(cpuFeatures);

#endif
}

Context::~Context()
{
#if defined(IPL_USE_MKL) && (defined(IPL_OS_WINDOWS) || defined(IPL_OS_MACOSX))
    mkl_free_buffers();
#endif
}

void Context::setDenormalsAreZeroes()
{
#if defined(IPL_ENABLE_IPP) && (defined(IPL_OS_WINDOWS) || defined(IPL_OS_LINUX) || (defined(IPL_OS_MACOSX) && defined(IPL_CPU_X64)))
    ippSetDenormAreZeros(1);
    ippSetFlushToZero(1, nullptr);
#endif
}

bool Context::isCallerAPIVersionAtLeast(uint32_t minMajor, uint32_t minMinor)
{
    auto callerMajor = (gAPIVersion() >> 16) & 0xff;
    auto callerMinor = (gAPIVersion() >> 8) & 0xff;

    return (callerMajor > minMajor || (callerMajor == minMajor && callerMinor >= minMinor));
}

Log& gLog()
{
    return Context::sLog;
}

Memory& gMemory()
{
    return Context::sMemory;
}

SIMDLevel gSIMDLevel()
{
    return Context::sSIMDLevel;
}

uint32_t gAPIVersion()
{
    return Context::sAPIVersion;
}

}
