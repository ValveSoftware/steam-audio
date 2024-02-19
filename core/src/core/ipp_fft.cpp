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

#include "fft.h"

#include <ipps.h>

#include "array.h"
#include "error.h"
#include "log.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// FFT
// --------------------------------------------------------------------------------------------------------------------

struct FFT::State
{
    IppsFFTSpec_R_32f* realSpec;
    IppsFFTSpec_C_32fc* complexSpec;
    Array<Ipp8u> specBuffer;
    Array<Ipp8u> applyBuffer;
};

FFT::FFT(int size,
         FFTDomain domain)
{
    numRealSamples = Math::nextpow2(size);
    numComplexSamples = (domain == FFTDomain::Real) ? (numRealSamples / 2 + 1) : numRealSamples;

    mState = make_unique<State>();
    mState->realSpec = nullptr;
    mState->complexSpec = nullptr;

    auto order = static_cast<int>(log2f(static_cast<float>(numRealSamples)));

    auto specSize = 0;
    auto initBufferSize = 0;
    auto applyBufferSize = 0;
    if (domain == FFTDomain::Real)
    {
        ippsFFTGetSize_R_32f(order, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &specSize, &initBufferSize, &applyBufferSize);
    }
    else
    {
        ippsFFTGetSize_C_32fc(order, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &specSize, &initBufferSize, &applyBufferSize);
    }

    mState->specBuffer.resize(specSize);
    mState->applyBuffer.resize(applyBufferSize);

    Array<Ipp8u> initBuffer(initBufferSize);

    if (domain == FFTDomain::Real)
    {
        ippsFFTInit_R_32f(&mState->realSpec, order, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone,
                          mState->specBuffer.data(), initBuffer.data());
    }
    else
    {
        ippsFFTInit_C_32fc(&mState->complexSpec, order, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone,
                           mState->specBuffer.data(), initBuffer.data());
    }
}

FFT::~FFT() = default;

void FFT::applyForward(const float* signal,
                                    complex_t* spectrum) const
{
    ippsFFTFwd_RToCCS_32f(signal, (Ipp32f*) spectrum, mState->realSpec, mState->applyBuffer.data());
}

void FFT::applyForward(const complex_t* signal,
                                    complex_t* spectrum) const
{
    ippsFFTFwd_CToC_32fc((const Ipp32fc*) signal, (Ipp32fc*) spectrum, mState->complexSpec, mState->applyBuffer.data());
}

void FFT::applyInverse(const complex_t* spectrum,
                                    float* signal) const
{
    ippsFFTInv_CCSToR_32f((const Ipp32f*) spectrum, signal, mState->realSpec, mState->applyBuffer.data());
}

void FFT::applyInverse(const complex_t* spectrum,
                                    complex_t* signal) const
{
    ippsFFTInv_CToC_32fc((const Ipp32fc*) spectrum, (Ipp32fc*) signal, mState->complexSpec, mState->applyBuffer.data());
}

}

#endif