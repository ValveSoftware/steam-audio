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

#if !defined(IPL_OS_MACOSX) || defined(IPL_CPU_ARM64)

#include "array_math.h"

#include "float4.h"
#include "platform.h"

namespace ipl {

void ArrayMath::add(int size,
                    const float* in1,
                    const float* in2,
                    float* out)
{
    auto simdSize = size & ~3;

    for (auto i = 0; i < simdSize; i += 4)
    {
        float4::store(&out[i], float4::add(float4::load(&in1[i]), float4::load(&in2[i])));
    }

    for (auto i = simdSize; i < size; ++i)
    {
        out[i] = in1[i] + in2[i];
    }
}

void ArrayMath::add(int size,
                    const complex_t* in1,
                    const complex_t* in2,
                    complex_t* out)
{
    add(2 * size, reinterpret_cast<const float*>(in1), reinterpret_cast<const float*>(in2),
        reinterpret_cast<float*>(out));
}

void ArrayMath::multiply(int size,
                         const float* in1,
                         const float* in2,
                         float* out)
{
    auto simdSize = size & ~3;

    for (auto i = 0; i < simdSize; i += 4)
    {
        float4::store(&out[i], float4::mul(float4::load(&in1[i]), float4::load(&in2[i])));
    }

    for (auto i = simdSize; i < size; ++i)
    {
        out[i] = in1[i] * in2[i];
    }
}

void ArrayMath::multiply(int size,
                         const complex_t* in1,
                         const complex_t* in2,
                         complex_t* out)
{
    // SIMD processing will be carried out on 4 elements at a time. So
    // the number of elements that will be processed using SIMD will be
    // the size of the array, rounded down to the nearest multiple of 4.
    // To calculate this size, we can simply zero out the least significant
    // two bits of size, which is achieved by ANDing size with ~3 (i.e., a
    // number with all bits except the least significant two bits set).
    auto arraySizeAsReal = 2 * size;
    auto simdArraySizeAsReal = arraySizeAsReal & ~3;

    auto in1Data = reinterpret_cast<const float*>(in1);
    auto in2Data = reinterpret_cast<const float*>(in2);
    auto outData = reinterpret_cast<float*>(out);

#if (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

    for (auto i = 0U; i < simdArraySizeAsReal; i += 4)
    {
        auto x1 = float4::load(&in1Data[i]);
        auto x2 = float4::load(&in2Data[i]);

        auto b0 = float4::set(-1.0f, 1.0f, -1.0f, 1.0f);
        auto b1 = _mm_shuffle_ps(x1, x1, _MM_SHUFFLE(2, 2, 0, 0));
        auto b3 = _mm_shuffle_ps(x1, x1, _MM_SHUFFLE(3, 3, 1, 1));
        auto b4 = _mm_shuffle_ps(x2, x2, _MM_SHUFFLE(2, 3, 0, 1));

        auto y = float4::add(float4::mul(b1, x2), float4::mul(b0, float4::mul(b3, b4)));

        float4::store(&outData[i], y);
    }

#elif (defined(IPL_CPU_ARMV7) || defined(IPL_CPU_ARM64))

    // Since we're using strided loads, we actually need to access 8 elements
    // at a time, so we need to zero out the least significant 3 bits of the
    // the array size.
    simdArraySizeAsReal = arraySizeAsReal & ~7;

    for (auto i = 0; i < simdArraySizeAsReal; i += 8)
    {
        auto a = vld2q_f32(&in1Data[i]);
        auto b = vld2q_f32(&in2Data[i]);
        float32x4x2_t c;

        c.val[0] = float4::sub(float4::mul(a.val[0], b.val[0]), float4::mul(a.val[1], b.val[1]));
        c.val[1] = float4::add(float4::mul(a.val[0], b.val[1]), float4::mul(a.val[1], b.val[0]));

        vst2q_f32(&outData[i], c);
    }

#endif

    for (auto i = simdArraySizeAsReal / 2; i < size; ++i)
    {
        out[i] = in1[i] * in2[i];
    }
}

void ArrayMath::multiplyAccumulate(int size,
                                   const float* in1,
                                   const float* in2,
                                   float* accum)
{
    auto simdSize = size & ~3;

    for (auto i = 0; i < simdSize; i += 4)
    {
        auto x1 = float4::load(&in1[i]);
        auto x2 = float4::load(&in2[i]);
        auto y = float4::load(&accum[i]);

        y = float4::add(y, float4::mul(x1, x2));

        float4::store(&accum[i], y);
    }

    for (auto i = simdSize; i < size; ++i)
    {
        accum[i] += in1[i] * in2[i];
    }
}

void ArrayMath::multiplyAccumulate(int size,
                                   const complex_t* in1,
                                   const complex_t* in2,
                                   complex_t* accum)
{
    auto arraySizeAsReal = 2 * size;
    auto simdArraySizeAsReal = arraySizeAsReal & ~3;

    auto in1Data = reinterpret_cast<const float*>(in1);
    auto in2Data = reinterpret_cast<const float*>(in2);
    auto outData = reinterpret_cast<float*>(accum);

#if (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

    for (auto i = 0U; i < simdArraySizeAsReal; i += 4)
    {
        auto x1 = float4::load(&in1Data[i]);
        auto x2 = float4::load(&in2Data[i]);

        auto b0 = float4::set(-1.0f, 1.0f, -1.0f, 1.0f);
        auto b1 = _mm_shuffle_ps(x1, x1, _MM_SHUFFLE(2, 2, 0, 0));
        auto b3 = _mm_shuffle_ps(x1, x1, _MM_SHUFFLE(3, 3, 1, 1));
        auto b4 = _mm_shuffle_ps(x2, x2, _MM_SHUFFLE(2, 3, 0, 1));

        auto y = float4::add(float4::mul(b1, x2), float4::mul(b0, float4::mul(b3, b4)));

        y = float4::add(y, float4::load(&outData[i]));

        float4::store(&outData[i], y);
    }

#elif (defined(IPL_CPU_ARMV7) || defined(IPL_CPU_ARM64))

    simdArraySizeAsReal = arraySizeAsReal & ~7;

    for (auto i = 0; i < simdArraySizeAsReal; i += 8)
    {
        auto a = vld2q_f32(&in1Data[i]);
        auto b = vld2q_f32(&in2Data[i]);
        auto c = vld2q_f32(&outData[i]);

        c.val[0] = float4::add(c.val[0], float4::sub(float4::mul(a.val[0], b.val[0]), float4::mul(a.val[1], b.val[1])));
        c.val[1] = float4::add(c.val[1], float4::add(float4::mul(a.val[0], b.val[1]), float4::mul(a.val[1], b.val[0])));

        vst2q_f32(&outData[i], c);
    }

#endif

    for (auto i = simdArraySizeAsReal / 2; i < size; ++i)
    {
        accum[i] += in1[i] * in2[i];
    }
}

void ArrayMath::scale(int size,
                      const float* in,
                      float scalar,
                      float* out)
{
    auto simdSize = size & ~3;
    auto simdScalar = float4::set1(scalar);

    for (auto i = 0; i < simdSize; i += 4)
    {
        float4::store(&out[i], float4::mul(float4::load(&in[i]), simdScalar));
    }

    for (auto i = simdSize; i < size; ++i)
    {
        out[i] = in[i] * scalar;
    }
}

void ArrayMath::scale(int size,
                      const complex_t* in,
                      float scalar,
                      complex_t* out)
{
    scale(2 * size, reinterpret_cast<const float*>(in), scalar, reinterpret_cast<float*>(out));
}

void ArrayMath::scaleAccumulate(int size,
                                const float* in,
                                float scalar,
                                float* out)
{
    auto simdSize = size & ~3;
    auto simdScalar = float4::set1(scalar);

    for (auto i = 0; i < simdSize; i += 4)
    {
        auto x = float4::load(&in[i]);
        auto y = float4::load(&out[i]);

        y = float4::add(y, float4::mul(x, simdScalar));

        float4::store(&out[i], y);
    }

    for (auto i = simdSize; i < size; ++i)
    {
        out[i] += scalar * in[i];
    }
}

void ArrayMath::addConstant(int size,
                            const float* in,
                            float constant,
                            float* out)
{
    auto simdSize = size & ~3;
    auto simdConstant = float4::set1(constant);

    for (auto i = 0; i < simdSize; i += 4)
    {
        float4::store(&out[i], float4::add(float4::load(&in[i]), simdConstant));
    }

    for (auto i = simdSize; i < size; ++i)
    {
        out[i] = in[i] + constant;
    }
}

void ArrayMath::max(int size,
                    const float* in,
                    float& out)
{
    out = in[0];
    for (auto i = 1; i < size; ++i)
    {
        out = std::max(out, in[i]);
    }
}

void ArrayMath::maxIndex(int size,
                         const float* in,
                         float& outValue,
                         int& outIndex)
{
    outIndex = 0;
    outValue = in[0];
    for (auto i = 1; i < size; ++i)
    {
        if (in[i] > outValue)
        {
            outIndex = i;
            outValue = in[i];
        }
    }
}

void ArrayMath::threshold(int size,
                          const float* in,
                          float minValue,
                          float* out)
{
    auto simdSize = size & ~3;
    auto simdMinValue = float4::set1(minValue);

    for (auto i = 0; i < simdSize; i += 4)
    {
        float4::store(&out[i], float4::max(float4::load(&in[i]), simdMinValue));
    }

    for (auto i = simdSize; i < size; ++i)
    {
        out[i] = std::max(in[i], minValue);
    }
}

void ArrayMath::log(int size,
                    const float* in,
                    float* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = logf(in[i]);
    }
}

void ArrayMath::exp(int size,
                    const float* in,
                    float* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = expf(in[i]);
    }
}

void ArrayMath::exp(int size,
                    const complex_t* in,
                    complex_t* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = exp(in[i]);
    }
}

void ArrayMath::magnitude(int size,
                          const complex_t* in,
                          float* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = abs(in[i]);
    }
}

void ArrayMath::phase(int size,
                      const complex_t* in,
                      float* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = arg(in[i]);
    }
}

void ArrayMath::polarToCartesian(int size,
                                 const float* inMagnitude,
                                 const float* inPhase,
                                 complex_t* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = std::polar(inMagnitude[i], inPhase[i]);
    }
}

}

#endif
