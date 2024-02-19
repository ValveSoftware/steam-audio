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

#include <arm_neon.h>

#include "platform.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// float4
// --------------------------------------------------------------------------------------------------------------------

typedef float32x4_t float4_t;

namespace float4
{
    // Returns a + b.
    inline float4_t add(float4_t a,
                        float4_t b)
    {
        return vaddq_f32(a, b);
    }

    // Returns a - b.
    inline float4_t sub(float4_t a,
                        float4_t b)
    {
        return vsubq_f32(a, b);
    }

    // Returns a * b.
    inline float4_t mul(float4_t a,
                        float4_t b)
    {
        return vmulq_f32(a, b);
    }

    // Returns 1 / x.
    inline float4_t rcp(float4_t x)
    {
        return vrecpeq_f32(x);
    }

    // Returns a / b.
    inline float4_t div(float4_t a,
                        float4_t b)
    {
        return mul(a, rcp(b));
    }

    // Returns min(a, b).
    inline float4_t min(float4_t a,
                        float4_t b)
    {
        return vminq_f32(a, b);
    }

    // Returns max(a, b).
    inline float4_t max(float4_t a,
                        float4_t b)
    {
        return vmaxq_f32(a, b);
    }

    // Returns 1 / sqrt(x).
    inline float4_t rsqrt(float4_t x)
    {
        return vrsqrteq_f32(x);
    }

    // Returns sqrt(x).
    inline float4_t sqrt(float4_t x)
    {
        return rcp(rsqrt(x));
    }

    // Bitwise AND.
    inline float4_t andbits(float4_t a,
                            float4_t b)
    {
        return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
    }

    // Bitwise OR.
    inline float4_t orbits(float4_t a,
                           float4_t b)
    {
        return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
    }

    // Bitwise AND-NOT.
    inline float4_t andnotbits(float4_t a,
                               float4_t b)
    {
        return vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
    }

    // Bitwise XOR.
    inline float4_t xorbits(float4_t a,
                            float4_t b)
    {
        return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
    }

    // Returns a mask indicating whether a > b.
    inline float4_t cmpgt(float4_t a,
                          float4_t b)
    {
        return vreinterpretq_f32_u32(vcgtq_f32(a, b));
    }

    // Returns a mask indicating whether a < b.
    inline float4_t cmplt(float4_t a,
                          float4_t b)
    {
        return vreinterpretq_f32_u32(vcltq_f32(a, b));
    }

    // Returns a mask indicating whether a >= b.
    inline float4_t cmpge(float4_t a,
                          float4_t b)
    {
        return vreinterpretq_f32_u32(vcgeq_f32(a, b));
    }

    // Returns a mask indicating whether a <= b.
    inline float4_t cmple(float4_t a,
                          float4_t b)
    {
        return vreinterpretq_f32_u32(vcleq_f32(a, b));
    }

    // Returns a mask indicating whether a == b.
    inline float4_t cmpeq(float4_t a,
                          float4_t b)
    {
        return vreinterpretq_f32_u32(vceqq_f32(a, b));
    }

    // Returns a mask indicating whether a != b.
    inline float4_t cmpneq(float4_t a,
                           float4_t b)
    {
        return orbits(cmpgt(a, b), cmplt(a, b));
    }

    // Round to nearest integer.
    inline float4_t round(float4_t a)
    {
        int32x4_t a_sign = vdupq_n_s32(1 << 31);
        int32x4_t a_05 = vreinterpretq_s32_f32(vdupq_n_f32(0.5f));
        int32x4_t a_addition = vorrq_s32(a_05, vandq_s32(a_sign, vreinterpretq_s32_f32(a)));

        return vreinterpretq_f32_s32(vreinterpretq_s32_f32(vaddq_f32(a, vreinterpretq_f32_s32(a_addition))));
    }

    // Aligned load.
    inline float4_t load(const float* p)
    {
        return vld1q_f32(p);
    }

    // Unaligned load.
    inline float4_t loadu(const float* p)
    {
        return vld1q_f32(p);
    }

    // Load a single value to all lanes.
    inline float4_t load1(const float* p)
    {
        return vld1q_dup_f32(p);
    }

    // Aligned store.
    inline void store(float* p,
                      float4_t x)
    {
        vst1q_f32(p, x);
    }

    // Unaligned store.
    inline void storeu(float* p,
                       float4_t x)
    {
        vst1q_f32(p, x);
    }

    // Set values of each lane.
    inline float4_t set(float w,
                        float x,
                        float y,
                        float z)
    {
        float elements[] = {w, x, y, z};
        return load(elements);
    }

    // Set values of all lanes to the same value.
    inline float4_t set1(float wxyz)
    {
        return vdupq_n_f32(wxyz);
    }

    // Set all lanes to zero.
    inline float4_t zero()
    {
        return set1(0.0f);
    }

    // Replicate lane N to all lanes.
    template <int N>
    inline float4_t replicate(float4_t in)
    {
    #if defined(IPL_CPU_ARM64)
        return vdupq_laneq_f32(in, N);
    #else
        return set1(vgetq_lane_f32(in, N));
    #endif
    }

    // Get the value in lane 0.
    inline float get1(float4_t in)
    {
        return vgetq_lane_f32(in, 0);
    }
}

}