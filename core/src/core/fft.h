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

#include "math_functions.h"
#include "memory_allocator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// FFT
// --------------------------------------------------------------------------------------------------------------------

// Fourier transforms can be calculated for real-valued data or complex-valued data. The Fourier transform of a
// complex-valued array is a complex-valued array of the same size; the Fourier transform of a real-valued array
// is a complex-valued array stored in conjugate-even form.
enum class FFTDomain
{
    Real,
    Complex
};

// Represents a discrete Fourier transform of a specific size.
class FFT
{
public:
    int numRealSamples; // Size of the real-valued data, rounded up to the next power of two.
    int numComplexSamples; // Size of the complex-valued spectrum.

    // Constructs a Fourier transform that applies to signals of a given size.
    FFT(int numRealSamples,
        FFTDomain domain = FFTDomain::Real);

    // Destructor. Required for pimpl.
    ~FFT();

    // Applies a forward Fourier transform. This converts a real-valued time-domain signal into a complex-valued
    // frequency-domain signal.
    void applyForward(const float* signal,
                      complex_t* spectrum) const;

    // Applies a forward Fourier transform. This converts a complex-valued time-domain signal into a complex-valued
    // frequency-domain signal.
    void applyForward(const complex_t* signal,
                      complex_t* spectrum) const;

    // Applies an inverse Fourier transform. This converts a complex-valued frequency-domain signal into a
    // real-valued time-domain signal. Appropriate scaling is applied so that a forward transform followed by an
    // inverse transform leaves the original signal intact.
    void applyInverse(const complex_t* spectrum,
                      float* signal) const;

    // Applies an inverse Fourier transform. This converts a complex-valued frequency-domain signal into a
    // complex-valued time-domain signal. Appropriate scaling is applied so that a forward transform followed by an
    // inverse transform leaves the original signal intact.
    void applyInverse(const complex_t* spectrum,
                      complex_t* signal) const;

private:
    struct State;
    unique_ptr<State> mState;
};

}
