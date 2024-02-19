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

#if defined(IPL_CPU_ARM64)

#include "array_math.h"

#include <Accelerate/Accelerate.h>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Array Math
// --------------------------------------------------------------------------------------------------------------------

static DSPSplitComplex getSplitComplex(const complex_t* in)
{
    DSPSplitComplex inSplit{};
    inSplit.realp = const_cast<float*>(&reinterpret_cast<const float*>(in)[0]);
    inSplit.imagp = const_cast<float*>(&reinterpret_cast<const float*>(in)[1]);
    return inSplit;
}

static DSPSplitComplex getSplitComplex(complex_t* in)
{
    DSPSplitComplex inSplit{};
    inSplit.realp = &reinterpret_cast<float*>(in)[0];
    inSplit.imagp = &reinterpret_cast<float*>(in)[1];
    return inSplit;
}

void ArrayMath::add(int size, const float* in1, const float* in2, float* out)
{
    vDSP_vadd(in1, 1, in2, 1, out, 1, size);
}

void ArrayMath::add(int size, const complex_t* in1, const complex_t* in2, complex_t* out)
{
    auto in1Split = getSplitComplex(in1);
    auto in2Split = getSplitComplex(in2);
    auto outSplit = getSplitComplex(out);

    vDSP_zvadd(&in1Split, 2, &in2Split, 2, &outSplit, 2, size);
}

void ArrayMath::multiply(int size, const float* in1, const float* in2, float* out)
{
    vDSP_vmul(in1, 1, in2, 1, out, 1, size);
}

void ArrayMath::multiply(int size, const complex_t* in1, const complex_t* in2, complex_t* out)
{
    auto in1Split = getSplitComplex(in1);
    auto in2Split = getSplitComplex(in2);
    auto outSplit = getSplitComplex(out);

    vDSP_zvmul(&in1Split, 2, &in2Split, 2, &outSplit, 2, size, 1 /* don't conjugate anything */);
}

void ArrayMath::multiplyAccumulate(int size, const float* in1, const float* in2, float* accum)
{
    vDSP_vma(in1, 1, in2, 1, accum, 1, accum, 1, size);
}

void ArrayMath::multiplyAccumulate(int size, const complex_t* in1, const complex_t* in2, complex_t* accum)
{
    auto in1Split = getSplitComplex(in1);
    auto in2Split = getSplitComplex(in2);
    auto accumSplit = getSplitComplex(accum);

    vDSP_zvma(&in1Split, 2, &in2Split, 2, &accumSplit, 2, &accumSplit, 2, size);
}

void ArrayMath::scale(int size, const float* in, float scalar, float* out)
{
    vDSP_vsmul(in, 1, &scalar, out, 1, size);
}

void ArrayMath::scale(int size, const complex_t* in, float scalar, complex_t* out)
{
    vDSP_vsmul(reinterpret_cast<const float*>(in), 1, &scalar, reinterpret_cast<float*>(out), 1, 2 * size);
}

void ArrayMath::scaleAccumulate(int size, const float* in, float scalar, float* out)
{
    vDSP_vsma(in, 1, &scalar, out, 1, out, 1, size);
}

void ArrayMath::addConstant(int size, const float* in, float constant, float* out)
{
    vDSP_vsadd(in, 1, &constant, out, 1, size);
}

void ArrayMath::max(int size, const float* in, float &out)
{
    vDSP_maxv(in, 1, &out, size);
}

void ArrayMath::maxIndex(int size, const float* in, float &outValue, int &outIndex)
{
    auto index = VDSP_Length{0};
    vDSP_maxvi(in, 1, &outValue, &index, size);
    outIndex = static_cast<int>(index);
}

void ArrayMath::threshold(int size, const float* in, float minValue, float* out)
{
    vDSP_vthr(in, 1, &minValue, out, 1, size);
}

void ArrayMath::log(int size, const float* in, float* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = logf(in[i]);
    }
}

void ArrayMath::exp(int size, const float* in, float* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = expf(in[i]);
    }
}

void ArrayMath::exp(int size, const complex_t* in, complex_t* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = exp(in[i]);
    }
}

void ArrayMath::magnitude(int size, const complex_t* in, float* out)
{
    auto inSplit = getSplitComplex(in);

    vDSP_zvabs(&inSplit, 2, out, 1, size);
}

void ArrayMath::phase(int size, const complex_t* in, float* out)
{
    auto inSplit = getSplitComplex(in);

    vDSP_zvphas(&inSplit, 2, out, 1, size);
}

void ArrayMath::polarToCartesian(int size, const float* inMagnitude, const float* inPhase, complex_t* out)
{
    for (auto i = 0; i < size; ++i)
    {
        out[i] = std::polar(inMagnitude[i], inPhase[i]);
    }
}

}

#endif
