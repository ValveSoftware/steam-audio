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

#if defined(IPL_ENABLE_FLOAT8)

#include <immintrin.h>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// float8
// --------------------------------------------------------------------------------------------------------------------

typedef __m256 float8_t;

namespace float8
{
    inline float8_t add(float8_t a,
                        float8_t b)
    {
        return _mm256_add_ps(a, b);
    }

    inline float8_t sub(float8_t a,
                        float8_t b)
    {
        return _mm256_sub_ps(a, b);
    }

    inline float8_t mul(float8_t a,
                        float8_t b)
    {
        return _mm256_mul_ps(a, b);
    }

    inline float8_t div(float8_t a,
                        float8_t b)
    {
        return _mm256_div_ps(a, b);
    }

    inline float8_t load(const float* p)
    {
        return _mm256_load_ps(p);
    }

    inline float8_t loadu(const float* p)
    {
        return _mm256_loadu_ps(p);
    }

    inline void store(float* p,
                      float8_t x)
    {
        return _mm256_store_ps(p, x);
    }

    inline void storeu(float* p,
                       float8_t x)
    {
        return _mm256_storeu_ps(p, x);
    }

    inline float8_t set(float x0,
                        float x1,
                        float x2,
                        float x3,
                        float x4,
                        float x5,
                        float x6,
                        float x7)
    {
        return _mm256_set_ps(x7, x6, x5, x4, x3, x2, x1, x0);
    }

    inline float8_t set1(float x)
    {
        return _mm256_set1_ps(x);
    }

    inline float8_t set1(const float* x)
    {
        return _mm256_broadcast_ss(x);
    }

    inline float8_t zero()
    {
        return _mm256_setzero_ps();
    }

    inline float get1(float8_t x)
    {
        return _mm_cvtss_f32(_mm256_castps256_ps128(x));
    }

    template <int N>
    inline float8_t replicateHalves(float8_t x)
    {
        return _mm256_shuffle_ps(x, x, _MM_SHUFFLE(N, N, N, N));
    }

    inline float8_t replicateLower(float8_t x)
    {
        return _mm256_permute2f128_ps(x, x, 0x00);
    }

    inline float8_t replicateUpper(float8_t x)
    {
        return _mm256_permute2f128_ps(x, x, 0x11);
    }

    inline void avoidTransitionPenalty()
    {
        _mm256_zeroupper();
    }
}

#if defined(IPL_OS_WINDOWS)
inline float8_t operator+(float8_t a,
                          float8_t b)
{
    return float8::add(a, b);
}

inline float8_t operator-(float8_t a,
                          float8_t b)
{
    return float8::sub(a, b);
}

inline float8_t operator*(float8_t a,
                          float8_t b)
{
    return float8::mul(a, b);
}

inline float8_t operator/(float8_t a,
                          float8_t b)
{
    return float8::div(a, b);
}
#endif

}

#endif
