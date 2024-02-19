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

#if defined(IPL_ENABLE_IPP) && (defined(IPL_OS_WINDOWS) || defined(IPL_OS_LINUX) || (defined(IPL_OS_MACOSX) && defined(IPL_CPU_X64)))

#include "array_math.h"

#include <ipp.h>
#include <ipps.h>

namespace ipl {

void ArrayMath::add(int size,
                    const float* in1,
                    const float* in2,
                    float* out)
{
    ippsAdd_32f(in1, in2, out, size);
}

void ArrayMath::add(int size,
                    const complex_t* in1,
                    const complex_t* in2,
                    complex_t* out)
{
    ippsAdd_32fc(reinterpret_cast<const Ipp32fc*>(in1), reinterpret_cast<const Ipp32fc*>(in2),
                 reinterpret_cast<Ipp32fc*>(out), size);
}

void ArrayMath::multiply(int size,
                         const float* in1,
                         const float* in2,
                         float* out)
{
    ippsMul_32f(in1, in2, out, size);
}

void ArrayMath::multiply(int size,
                         const complex_t* in1,
                         const complex_t* in2,
                         complex_t* out)
{
    ippsMul_32fc(reinterpret_cast<const Ipp32fc*>(in1), reinterpret_cast<const Ipp32fc*>(in2),
                 reinterpret_cast<Ipp32fc*>(out), size);
}

void ArrayMath::multiplyAccumulate(int size,
                                   const float* in1,
                                   const float* in2,
                                   float* accum)
{
    ippsAddProduct_32f(in1, in2, accum, size);
}

void ArrayMath::multiplyAccumulate(int size,
                                   const complex_t* in1,
                                   const complex_t* in2,
                                   complex_t* accum)
{
    ippsAddProduct_32fc(reinterpret_cast<const Ipp32fc*>(in1), reinterpret_cast<const Ipp32fc*>(in2),
                        reinterpret_cast<Ipp32fc*>(accum), size);
}

void ArrayMath::scale(int size,
                      const float* in,
                      float scalar,
                      float* out)
{
    ippsMulC_32f(in, scalar, out, size);
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
    ippsAddProductC_32f(in, scalar, out, size);
}

void ArrayMath::addConstant(int size,
                            const float* in,
                            float constant,
                            float* out)
{
    ippsAddC_32f(in, constant, out, size);
}

void ArrayMath::max(int size,
                    const float* in,
                    float& out)
{
    ippsMax_32f(in, size, &out);
}

void ArrayMath::maxIndex(int size,
                         const float* in,
                         float& outValue,
                         int& outIndex)
{
    ippsMaxIndx_32f(in, size, &outValue, &outIndex);
}

void ArrayMath::threshold(int size,
                          const float* in,
                          float minValue,
                          float* out)
{
    ippsThreshold_LT_32f(in, out, size, minValue);
}

void ArrayMath::log(int size,
                    const float* in,
                    float* out)
{
    ippsLn_32f(in, out, size);
}

void ArrayMath::exp(int size,
                    const float* in,
                    float* out)
{
    ippsExp_32f(in, out, size);
}

void ArrayMath::exp(int size,
                    const complex_t* in,
                    complex_t* out)
{
    ippsExp_32fc_A11(reinterpret_cast<const Ipp32fc*>(in), reinterpret_cast<Ipp32fc*>(out), size);
}

void ArrayMath::magnitude(int size,
                          const complex_t* in,
                          float* out)
{
    ippsMagnitude_32fc(reinterpret_cast<const Ipp32fc*>(in), out, size);
}

void ArrayMath::phase(int size,
                      const complex_t* in,
                      float* out)
{
    ippsArg_32fc_A11(reinterpret_cast<const Ipp32fc*>(in), out, size);
}

void ArrayMath::polarToCartesian(int size,
                                 const float* inMagnitude,
                                 const float* inPhase,
                                 complex_t* out)
{
    ippsPolarToCart_32fc(inMagnitude, inPhase, reinterpret_cast<Ipp32fc*>(out), size);
}

}

#endif
