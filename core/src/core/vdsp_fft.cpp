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

#include "fft.h"

#include <Accelerate/Accelerate.h>

#include "array.h"
#include "array_math.h"
#include "error.h"
#include "log.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// FFT
// --------------------------------------------------------------------------------------------------------------------

struct FFT::State
{
    vDSP_DFT_Setup  mForwardSetup;
    vDSP_DFT_Setup  mInverseSetup;
    Array<float, 2> mDeinterleaved;
};

FFT::FFT(int size, FFTDomain domain)
{
    numRealSamples = Math::nextpow2(size);
    numComplexSamples = (domain == FFTDomain::Real) ? (numRealSamples / 2 + 1) : numRealSamples;

    mState = ipl::make_unique<State>();

    if (domain == FFTDomain::Real)
    {
        mState->mForwardSetup = vDSP_DFT_zrop_CreateSetup(nullptr, numRealSamples, vDSP_DFT_FORWARD);
        if (!mState->mForwardSetup)
        {
            gLog().message(MessageSeverity::Error, "Unable to create vDSP forward DFT setup (size == %d).", size);
            throw Exception(Status::Initialization);
        }

        mState->mInverseSetup = vDSP_DFT_zrop_CreateSetup(nullptr, numRealSamples, vDSP_DFT_INVERSE);
        if (!mState->mInverseSetup)
        {
            gLog().message(MessageSeverity::Error, "Unable to create vDSP inverse DFT setup (size == %d).", size);
            throw Exception(Status::Initialization);
        }

        mState->mDeinterleaved.resize(2, numComplexSamples);
    }
    else
    {
        mState->mForwardSetup = vDSP_DFT_zop_CreateSetup(nullptr, numRealSamples, vDSP_DFT_FORWARD);
        if (!mState->mForwardSetup)
        {
            gLog().message(MessageSeverity::Error, "Unable to create vDSP forward DFT setup (size == %d).", size);
            throw Exception(Status::Initialization);
        }

        mState->mInverseSetup = vDSP_DFT_zop_CreateSetup(nullptr, numRealSamples, vDSP_DFT_INVERSE);
        if (!mState->mInverseSetup)
        {
            gLog().message(MessageSeverity::Error, "Unable to create vDSP inverse DFT setup (size == %d).", size);
            throw Exception(Status::Initialization);
        }

        mState->mDeinterleaved.resize(2, numComplexSamples);
    }
}

FFT::~FFT()
{
    vDSP_DFT_DestroySetup(mState->mInverseSetup);
    vDSP_DFT_DestroySetup(mState->mForwardSetup);
}

void FFT::applyForward(const float* signal, complex_t* spectrum) const
{
    DSPSplitComplex splitComplex{};
    splitComplex.realp = mState->mDeinterleaved[0];
    splitComplex.imagp = mState->mDeinterleaved[1];

    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(signal), 2, &splitComplex, 1, numRealSamples / 2);

    vDSP_DFT_Execute(mState->mForwardSetup, splitComplex.realp, splitComplex.imagp, splitComplex.realp, splitComplex.imagp);

    vDSP_ztoc(&splitComplex, 1, reinterpret_cast<DSPComplex*>(spectrum), 2, numRealSamples / 2);

    spectrum[numComplexSamples - 1].real(spectrum[0].imag());
    spectrum[0].imag(0.0f);

    ArrayMath::scale(numComplexSamples, spectrum, 0.5f, spectrum);
}

void FFT::applyForward(const complex_t* signal, complex_t* spectrum) const
{
    DSPSplitComplex splitComplex{};
    splitComplex.realp = mState->mDeinterleaved[0];
    splitComplex.imagp = mState->mDeinterleaved[1];

    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(signal), 2, &splitComplex, 1, numRealSamples);

    vDSP_DFT_Execute(mState->mForwardSetup, splitComplex.realp, splitComplex.imagp, splitComplex.realp, splitComplex.imagp);

    vDSP_ztoc(&splitComplex, 1, reinterpret_cast<DSPComplex*>(spectrum), 2, numRealSamples);
}

void FFT::applyInverse(const complex_t* spectrum, float* signal) const
{
    DSPSplitComplex splitComplex{};
    splitComplex.realp = mState->mDeinterleaved[0];
    splitComplex.imagp = mState->mDeinterleaved[1];

    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(spectrum), 2, &splitComplex, 1, numComplexSamples);

    splitComplex.imagp[0] = splitComplex.realp[numComplexSamples - 1];
    splitComplex.realp[numComplexSamples - 1] = 0.0f;

    vDSP_DFT_Execute(mState->mInverseSetup, splitComplex.realp, splitComplex.imagp, splitComplex.realp, splitComplex.imagp);

    vDSP_ztoc(&splitComplex, 1, reinterpret_cast<DSPComplex*>(signal), 2, numRealSamples / 2);

    ArrayMath::scale(numRealSamples, signal, 1.0f / numRealSamples, signal);
}

void FFT::applyInverse(const complex_t* spectrum, complex_t* signal) const
{
    DSPSplitComplex splitComplex{};
    splitComplex.realp = mState->mDeinterleaved[0];
    splitComplex.imagp = mState->mDeinterleaved[1];

    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(spectrum), 2, &splitComplex, 1, numRealSamples);

    vDSP_DFT_Execute(mState->mInverseSetup, splitComplex.realp, splitComplex.imagp, splitComplex.realp, splitComplex.imagp);

    vDSP_ztoc(&splitComplex, 1, reinterpret_cast<DSPComplex*>(signal), 2, numRealSamples);

    ArrayMath::scale(numRealSamples, signal, 1.0f / numRealSamples, signal);
}

}

#endif
