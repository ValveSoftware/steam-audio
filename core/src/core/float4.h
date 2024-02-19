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

#include "platform.h"

// --------------------------------------------------------------------------------------------------------------------
// float4
// --------------------------------------------------------------------------------------------------------------------

#if defined(IPL_CPU_X86) || defined(IPL_CPU_X64)
#include "sse_float4.h"
#elif (defined(IPL_CPU_ARMV7) || defined(IPL_CPU_ARM64))
#include "neon_float4.h"
#endif

#if defined(IPL_OS_WINDOWS)

namespace ipl {

inline float4_t operator+(float4_t a,
                          float4_t b)
{
    return float4::add(a, b);
}

inline float4_t operator-(float4_t a,
                          float4_t b)
{
    return float4::sub(a, b);
}

inline float4_t operator*(float4_t a,
                          float4_t b)
{
    return float4::mul(a, b);
}

inline float4_t operator/(float4_t a,
                          float4_t b)
{
    return float4::div(a, b);
}

}

#endif
