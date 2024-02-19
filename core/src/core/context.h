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

#include "log.h"
#include "memory_allocator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Context
// --------------------------------------------------------------------------------------------------------------------

enum class SIMDLevel
{
    SSE2,
    SSE4,
    AVX,
    AVX2,
    AVX512,

    NEON = SSE2
};

class Context
{
public:
    static Log sLog;
    static Memory sMemory;
    static SIMDLevel sSIMDLevel;
    static uint32_t sAPIVersion;

    Context(LogCallback logCallback,
            AllocateCallback allocateCallback,
            FreeCallback freeCallback,
            SIMDLevel simdLevel,
            uint32_t apiVersion);

    ~Context();

    static void setDenormalsAreZeroes();

    static bool isCallerAPIVersionAtLeast(uint32_t major, uint32_t minor);
};

Log& gLog();

Memory& gMemory();

SIMDLevel gSIMDLevel();

uint32_t gAPIVersion();

}
