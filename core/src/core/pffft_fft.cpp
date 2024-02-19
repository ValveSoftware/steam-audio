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

#include "fft.h"

#include <pffft.h>

#include "array.h"
#include "array_math.h"
#include "error.h"
#include "float4.h"
#include "log.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// FFT
// --------------------------------------------------------------------------------------------------------------------

struct FFT::State
{
    PFFFT_Setup* setup;
    Array<float> work;
    Array<float> signalReal;
    Array<complex_t> signalComplex;
    Array<complex_t> spectrum;
};

FFT::FFT(int size,
         FFTDomain domain)
{
    numRealSamples = Math::nextpow2(size);
    numComplexSamples = (domain == FFTDomain::Real) ? (numRealSamples / 2 + 1) : numRealSamples;

    mState = make_unique<State>();

    mState->setup = pffft_new_setup(numRealSamples, (domain == FFTDomain::Real) ? PFFFT_REAL : PFFFT_COMPLEX);
    if (!mState->setup)
    {
        gLog().message(MessageSeverity::Error, "Unable to create PFFFT setup (size == %d).", numRealSamples);
        throw Exception(Status::Initialization);
    }

    mState->work.resize((domain == FFTDomain::Real) ? numRealSamples : (2 * numRealSamples));
    mState->signalReal.resize(numRealSamples);
    mState->signalComplex.resize(numRealSamples);
    mState->spectrum.resize(numComplexSamples);
}

FFT::~FFT()
{
    pffft_destroy_setup(mState->setup);
}

void FFT::applyForward(const float* signal,
                       complex_t* spectrum) const
{
    memcpy(mState->signalReal.data(), signal, numRealSamples * sizeof(float));
    pffft_transform_ordered(mState->setup, mState->signalReal.data(), reinterpret_cast<float*>(mState->spectrum.data()), mState->work.data(), PFFFT_FORWARD);
    memcpy(spectrum, mState->spectrum.data(), numComplexSamples * sizeof(complex_t));

    spectrum[numComplexSamples - 1].real(spectrum[0].imag());
    spectrum[0].imag(0.0f);
}

void FFT::applyForward(const complex_t* signal,
                       complex_t* spectrum) const
{
    memcpy(mState->signalComplex.data(), signal, numRealSamples * sizeof(complex_t));
    pffft_transform_ordered(mState->setup, reinterpret_cast<const float*>(mState->signalComplex.data()), reinterpret_cast<float*>(mState->spectrum.data()), mState->work.data(), PFFFT_FORWARD);
    memcpy(spectrum, mState->spectrum.data(), numComplexSamples * sizeof(complex_t));
}

void FFT::applyInverse(const complex_t* spectrum,
                       float* signal) const
{
    memcpy(mState->spectrum.data(), spectrum, numComplexSamples * sizeof(complex_t));

    mState->spectrum[0].imag(mState->spectrum[numComplexSamples - 1].real());
    mState->spectrum[numComplexSamples - 1].real(0.0f);

    pffft_transform_ordered(mState->setup, reinterpret_cast<const float*>(mState->spectrum.data()), mState->signalReal.data(), mState->work.data(), PFFFT_BACKWARD);
    memcpy(signal, mState->signalReal.data(), numRealSamples * sizeof(float));
    ArrayMath::scale(numRealSamples, signal, 1.0f / numRealSamples, signal);
}

void FFT::applyInverse(const complex_t* spectrum,
                       complex_t* signal) const
{
    memcpy(mState->spectrum.data(), spectrum, numComplexSamples * sizeof(complex_t));
    pffft_transform_ordered(mState->setup, reinterpret_cast<const float*>(mState->spectrum.data()), reinterpret_cast<float*>(mState->signalComplex.data()), mState->work.data(), PFFFT_BACKWARD);
    memcpy(signal, mState->signalComplex.data(), numRealSamples * sizeof(complex_t));
    ArrayMath::scale(2 * numRealSamples, (float*) signal, 1.0f / numRealSamples, (float*) signal);
}

}

#endif
