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

#include <emmintrin.h>
#include <xmmintrin.h>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// float4
// --------------------------------------------------------------------------------------------------------------------

typedef __m128 float4_t;

namespace float4
{
    // Returns a + b.
    inline float4_t add(float4_t a,
                        float4_t b)
    {
        return _mm_add_ps(a, b);
    }

    // Returns a - b.
    inline float4_t sub(float4_t a,
                        float4_t b)
    {
        return _mm_sub_ps(a, b);
    }

    // Returns a * b.
    inline float4_t mul(float4_t a,
                        float4_t b)
    {
        return _mm_mul_ps(a, b);
    }

    // Returns a / b.
    inline float4_t div(float4_t a,
                        float4_t b)
    {
        return _mm_div_ps(a, b);
    }

    // Returns min(a, b).
    inline float4_t min(float4_t a,
                        float4_t b)
    {
        return _mm_min_ps(a, b);
    }

    // Returns max(a, b).
    inline float4_t max(float4_t a,
                        float4_t b)
    {
        return _mm_max_ps(a, b);
    }

    // Returns sqrt(x).
    inline float4_t sqrt(float4_t x)
    {
        return _mm_sqrt_ps(x);
    }

    // Returns 1 / x.
    inline float4_t rcp(float4_t x)
    {
        return _mm_rcp_ps(x);
    }

    // Returns 1 / sqrt(x).
    inline float4_t rsqrt(float4_t x)
    {
        return _mm_rsqrt_ps(x);
    }

    // Returns a mask indicating whether a > b.
    inline float4_t cmpgt(float4_t a,
                          float4_t b)
    {
        return _mm_cmpgt_ps(a, b);
    }

    // Returns a mask indicating whether a < b.
    inline float4_t cmplt(float4_t a,
                          float4_t b)
    {
        return _mm_cmplt_ps(a, b);
    }

    // Returns a mask indicating whether a >= b.
    inline float4_t cmpge(float4_t a,
                          float4_t b)
    {
        return _mm_cmpge_ps(a, b);
    }

    // Returns a mask indicating whether a <= b.
    inline float4_t cmple(float4_t a,
                          float4_t b)
    {
        return _mm_cmple_ps(a, b);
    }

    // Returns a mask indicating whether a == b.
    inline float4_t cmpeq(float4_t a,
                          float4_t b)
    {
        return _mm_cmpeq_ps(a, b);
    }

    // Returns a mask indicating whether a != b.
    inline float4_t cmpneq(float4_t a,
                           float4_t b)
    {
        return _mm_cmpneq_ps(a, b);
    }

    // Bitwise AND.
    inline float4_t andbits(float4_t a,
                            float4_t b)
    {
        return _mm_and_ps(a, b);
    }

    // Bitwise OR.
    inline float4_t orbits(float4_t a,
                           float4_t b)
    {
        return _mm_or_ps(a, b);
    }

    // Bitwise AND-NOT.
    inline float4_t andnotbits(float4_t a,
                               float4_t b)
    {
        return _mm_andnot_ps(a, b);
    }

    // Bitwise XOR.
    inline float4_t xorbits(float4_t a,
                            float4_t b)
    {
        return _mm_xor_ps(a, b);
    }

    // Round to nearest integer.
    inline float4_t round(float4_t a)
    {
        return _mm_cvtepi32_ps(_mm_cvtps_epi32(a));
    }

    // Aligned load.
    inline float4_t load(const float* p)
    {
        return _mm_load_ps(p);
    }

    // Unaligned load.
    inline float4_t loadu(const float* p)
    {
        return _mm_loadu_ps(p);
    }

    // Load a single value to all lanes.
    inline float4_t load1(const float* p)
    {
        return _mm_load1_ps(p);
    }

    // Aligned store.
    inline void store(float* p,
                      float4_t x)
    {
        _mm_store_ps(p, x);
    }

    // Unaligned store.
    inline void storeu(float* p,
                       float4_t x)
    {
        _mm_storeu_ps(p, x);
    }

    // Set values of each lane.
    inline float4_t set(float w,
                        float x,
                        float y,
                        float z)
    {
        return _mm_set_ps(z, y, x, w);
    }

    // Set values of all lanes to the same value.
    inline float4_t set1(float wxyz)
    {
        return _mm_set1_ps(wxyz);
    }

    // Set all lanes to zero.
    inline float4_t zero()
    {
        return _mm_setzero_ps();
    }

    // Replicate lane N to all lanes.
    template <int N>
    inline float4_t replicate(float4_t in)
    {
        return _mm_shuffle_ps(in, in, _MM_SHUFFLE(N, N, N, N));
    }

    // Get the value in lane 0.
    inline float get1(float4_t in)
    {
        return _mm_cvtss_f32(in);
    }
}

}