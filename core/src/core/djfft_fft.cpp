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

#include "fft.h"

#define DJ_FFT_IMPLEMENTATION
#include "dj_fft.h"

#include "array.h"
#include "array_math.h"
#include "error.h"
#include "float4.h"
#include "log.h"

#include <vector>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// FFT
// --------------------------------------------------------------------------------------------------------------------

struct FFT::State
{
    std::vector<float> work;
    std::vector<float> signalReal;
    std::vector<complex_t> signalComplex;
    std::vector<complex_t> spectrum;
};

FFT::FFT(int size,
         FFTDomain domain)
{
    numRealSamples = Math::nextpow2(size);
    numComplexSamples = (domain == FFTDomain::Real) ? (numRealSamples / 2 + 1) : numRealSamples;

    mState = make_unique<State>();

    mState->work.resize((domain == FFTDomain::Real) ? numRealSamples : (2 * numRealSamples));
    mState->signalReal.resize(numRealSamples);
    mState->signalComplex.resize(numRealSamples);
    mState->spectrum.resize(numComplexSamples);
}

FFT::~FFT()
{

}

void FFT::applyForward(const float* signal,
                       complex_t* spectrum) const
{
    for (int i = 0; i < numRealSamples; ++i)
        mState->signalComplex[i] = {signal[i], 0.0f};

    mState->spectrum = dj::fft1d(mState->signalComplex, dj::fft_dir::DIR_FWD);
    std::copy(mState->spectrum.begin(), mState->spectrum.end(), spectrum);
    spectrum[numComplexSamples - 1].real(spectrum[0].imag());
    spectrum[0].imag(0.0f);
}

void FFT::applyForward(const complex_t* signal,
                       complex_t* spectrum) const
{
    mState->spectrum = dj::fft1d(mState->signalComplex, dj::fft_dir::DIR_FWD);
    std::copy(mState->spectrum.begin(), mState->spectrum.end(), spectrum);
}

void FFT::applyInverse(const complex_t* spectrum,
                       float* signal) const
{
            std::copy(spectrum, spectrum + numComplexSamples, mState->spectrum.begin());

    mState->spectrum[0].imag(mState->spectrum[numComplexSamples - 1].real());
    mState->spectrum[numComplexSamples - 1].real(0.0f);

    auto signalComplex = dj::fft1d(mState->spectrum, dj::fft_dir::DIR_BWD);
    for (int i = 0; i < numRealSamples; ++i)
        signal[i] = signalComplex[i].real() / numRealSamples;

    ArrayMath::scale(numRealSamples, signal, 1.0f / numRealSamples, signal);
}

void FFT::applyInverse(const complex_t* spectrum,
                       complex_t* signal) const
{
            std::copy(spectrum, spectrum + numComplexSamples, mState->spectrum.begin());
    auto signalComplex = dj::fft1d(mState->spectrum, dj::fft_dir::DIR_BWD);
memcpy(signal, signalComplex.data(), numRealSamples * sizeof(complex_t));
    ArrayMath::scale(2 * numRealSamples, (float*) signal, 1.0f / numRealSamples, (float*) signal);
}

}
