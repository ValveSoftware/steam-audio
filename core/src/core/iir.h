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

#include "float4.h"
#include "memory_allocator.h"

#if defined(IPL_ENABLE_FLOAT8)
#include "float8.h"
#endif

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IIR
// --------------------------------------------------------------------------------------------------------------------

// Represents a biquad IIR filter, that can be used to carry out various filtering operations on RealSignals. Such a
// filter is essentially a recurrence relation: sample N of the filtered output signal depends on samples N, N-1, and
// N-2 of the input, as well as samples N-1 and N-2 of the _output_.
class IIR
{
public:
    float a1;   // Denominator coefficients (a0 is normalized to 1).
    float a2;
    float b0;   // Numerator coefficients.
    float b1;
    float b2;

    // Calculates the Fourier coefficient of the IIR filter's frequency response at the given angular frequency. Useful
    // for debug visualization of the frequency response.
    float spectrum(float frequency);

    // Creates a low-pass filter (removes all frequencies above the cutoff).
    static IIR lowPass(float cutoffFrequency,
                       int samplingRate);

    // Creates a high-pass filter (removes all frequencies below the cutoff).
    static IIR highPass(float cutoffFrequency,
                        int samplingRate);

    // Creates a band-pass filter (removes all frequencies outside the cutoffs).
    static IIR bandPass(float lowCutoffFrequency,
                        float highCutoffFrequency,
                        int samplingRate);

    // Creates a low-shelf filter (controls the amplitude of all frequencies below the cutoff).
    static IIR lowShelf(float cutoffFrequency,
                        float gain,
                        int samplingRate);

    // Creates a high-shelf filter (controls the amplitude of all frequencies above the cutoff).
    static IIR highShelf(float cutoffFrequency,
                         float gain,
                         int samplingRate);

    // Creates a peaking filter (controls the amplitude of all frequencies between the cutoffs).
    static IIR peaking(float lowCutoffFrequency,
                       float highCutoffFrequency,
                       float gain,
                       int samplingRate);
};


// --------------------------------------------------------------------------------------------------------------------
// IIRFilterer
// --------------------------------------------------------------------------------------------------------------------

// State required for filtering a signal with an IIR filter over multiple frames. Ensures continuity between frames
// when the filter doesn't change between frames. If the filter _does_ change, the caller must implement
// crossfading or some other approach to ensure smoothness.
class IIRFilterer
{
public:
    // Default constructor initializes the filter to emit silence given any input.
    IIRFilterer();

    // Resets internal filter state.
    void reset();

    // Copy state from another filter.
    void copyState(const IIRFilterer& other);

    // Resets IIR filter coefficients.
    void resetFilter();

    // Specifies the IIR filter coefficients to use when filtering.
    void setFilter(const IIR& filter);

    // Applies the filter to a single sample of input.
    float apply(float in);

    // Applies the filter to 4 samples of input, using SIMD operations.
    float4_t apply(float4_t in);

#if defined(IPL_ENABLE_FLOAT8)
    // Applies the filter to 8 samples of input, using SIMD operations.
    float8_t apply(float8_t in);
#endif

    // Applies the filter to an entire buffer of input, using SIMD operations.
    void apply(int size,
               const float* in,
               float* out);

private:
    IIR mFilter; // The IIR filter to apply.
    float mXm1; // Input value from 1 sample ago.
    float mXm2; // Input value from 2 samples ago.
    float mYm1; // Output value from 1 sample ago.
    float mYm2; // Output value from 2 samples ago.
    alignas(float4_t) float mCoeffs4[8][4]; // Filter coefficient matrix for SIMD acceleration, precomputed in setFilter.
#if defined(IPL_ENABLE_FLOAT8)
    alignas(float8_t) float mCoeffs8[12][8]; // Filter coefficient matrix for SIMD acceleration, precomputed in setFilter.
#endif

    void (IIRFilterer::* mResetFilterDispatch)();
    void (IIRFilterer::* mSetFilterDispatch)(const IIR& filter);
    void (IIRFilterer::* mApplyDispatch)(int, const float*, float*);

    void resetFilter_float4();
    void setFilter_float4(const IIR& filter);
    void apply_float4(int size, const float* in, float* out);

#if defined(IPL_ENABLE_FLOAT8)
    void resetFilter_float8();
    void setFilter_float8(const IIR& filter);
    void apply_float8(int size, const float* in, float* out);
#endif
};

}
