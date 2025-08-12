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
class IIR2
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
    static IIR2 lowPass(float cutoffFrequency,
                       int samplingRate);

    static IIR2 lowPass(float cutoffFrequency,
                       float Q,
                       int samplingRate);

    // Creates a high-pass filter (removes all frequencies below the cutoff).
    static IIR2 highPass(float cutoffFrequency,
                        int samplingRate);

    static IIR2 highPass(float cutoffFrequency,
                        float Q,
                        int samplingRate);

    // Creates a band-pass filter (removes all frequencies outside the cutoffs).
    static IIR2 bandPass(float lowCutoffFrequency,
                        float highCutoffFrequency,
                        int samplingRate);

    // Creates a low-shelf filter (controls the amplitude of all frequencies below the cutoff).
    static IIR2 lowShelf(float cutoffFrequency,
                        float gain,
                        int samplingRate);

    static IIR2 lowShelf(float cutoffFrequency,
                        float Q,
                        float gain,
                        int samplingRate);

    // Creates a high-shelf filter (controls the amplitude of all frequencies above the cutoff).
    static IIR2 highShelf(float cutoffFrequency,
                         float gain,
                         int samplingRate);

    static IIR2 highShelf(float cutoffFrequency,
                         float Q,
                         float gain,
                         int samplingRate);

    // Creates a peaking filter (controls the amplitude of all frequencies between the cutoffs).
    static IIR2 peaking(float lowCutoffFrequency,
                       float highCutoffFrequency,
                       float gain,
                       int samplingRate);

    static void bandFilters(IIR2* filters, int samplingRate);
};


// --------------------------------------------------------------------------------------------------------------------
// IIRFilterer
// --------------------------------------------------------------------------------------------------------------------

// State required for filtering a signal with an IIR filter over multiple frames. Ensures continuity between frames
// when the filter doesn't change between frames. If the filter _does_ change, the caller must implement
// crossfading or some other approach to ensure smoothness.
class IIR2Filterer
{
public:
    // Default constructor initializes the filter to emit silence given any input.
    IIR2Filterer();

    // Resets internal filter state.
    void reset();

    // Copy state from another filter.
    void copyState(const IIR2Filterer& other);

    // Resets IIR filter coefficients.
    void resetFilter();

    // Specifies the IIR filter coefficients to use when filtering.
    void setFilter(const IIR2& filter);

    // Applies the filter to a single sample of input.
    float apply(float in);

    // Applies the filter to 4 samples of input, using SIMD operations.
    float4_t apply(float4_t in);

#if defined(IPL_ENABLE_FLOAT8)
    // Applies the filter to 8 samples of input, using SIMD operations.
    float8_t IPL_FLOAT8_ATTR apply(float8_t in);
#endif

    // Applies the filter to an entire buffer of input, using SIMD operations.
    void apply(int size,
               const float* in,
               float* out);

private:
    IIR2 mFilter; // The IIR filter to apply.
    float mXm1; // Input value from 1 sample ago.
    float mXm2; // Input value from 2 samples ago.
    float mYm1; // Output value from 1 sample ago.
    float mYm2; // Output value from 2 samples ago.
    alignas(float4_t) float mCoeffs4[8][4]; // Filter coefficient matrix for SIMD acceleration, precomputed in setFilter.
#if defined(IPL_ENABLE_FLOAT8)
    alignas(float8_t) float mCoeffs8[12][8]; // Filter coefficient matrix for SIMD acceleration, precomputed in setFilter.
#endif

    void (IIR2Filterer::* mResetFilterDispatch)();
    void (IIR2Filterer::* mSetFilterDispatch)(const IIR2& filter);
    void (IIR2Filterer::* mApplyDispatch)(int, const float*, float*);

    void resetFilter_float4();
    void setFilter_float4(const IIR2& filter);
    void apply_float4(int size, const float* in, float* out);

#if defined(IPL_ENABLE_FLOAT8)
    void IPL_FLOAT8_ATTR resetFilter_float8();
    void IPL_FLOAT8_ATTR setFilter_float8(const IIR2& filter);
    void IPL_FLOAT8_ATTR apply_float8(int size, const float* in, float* out);
#endif
};


// --------------------------------------------------------------------------------------------------------------------
// IIR8
// --------------------------------------------------------------------------------------------------------------------

class IIR8
{
public:
    float spectrum(float frequency);
    float spectrumPeak();

    static IIR8 lowPass(float cutoff, int samplingRate);
    static IIR8 highPass(float cutoff, int samplingRate);
    static IIR8 bandPass(float lowCutoff, float highCutoff, int samplingRate);
    static IIR8 lowShelf(float cutoff, float gain, int samplingRate);
    static IIR8 highShelf(float cutoff, float gain, int samplingRate);
    static IIR8 peaking(float lowCutoff, float highCutoff, float gain, int samplingRate);
    static void bandFilters(IIR8* filters, int samplingRate);

public:
    IIR2 m_iir[4];
};


// --------------------------------------------------------------------------------------------------------------------
// IIR8Filterer
// --------------------------------------------------------------------------------------------------------------------

class IIR8Filterer
{
public:
    void reset();
    void copyState(const IIR8Filterer& other);

    void resetFilter();
    void setFilter(const IIR8& filter);

    float apply(float in);
    float4_t apply(float4_t in);
#if defined(IPL_ENABLE_FLOAT8)
    float8_t IPL_FLOAT8_ATTR apply(float8_t in);
#endif
    void apply(int size, const float* in, float* out);

private:
    IIR2Filterer m_filterers[4];
};


// --------------------------------------------------------------------------------------------------------------------
// IIR
// --------------------------------------------------------------------------------------------------------------------

class IIR
{
public:
    static bool sUseOrder8;

    float spectrum(float frequency);

    static IIR lowPass(float cutoff, int samplingRate);
    static IIR highPass(float cutoff, int samplingRate);
    static IIR bandPass(float lowCutoff, float highCutoff, int samplingRate);
    static IIR lowShelf(float cutoff, float gain, int samplingRate);
    static IIR highShelf(float cutoff, float gain, int samplingRate);
    static IIR peaking(float lowCutoff, float highCutoff, float gain, int samplingRate);
    static void bandFilters(IIR* filters, int samplingRate);

public:
    IIR2 m_iir2;
    IIR8 m_iir8;
};


// --------------------------------------------------------------------------------------------------------------------
// IIRFilterer
// --------------------------------------------------------------------------------------------------------------------

class IIRFilterer
{
public:
    static bool sEnableSwitching;

    IIRFilterer();

    void reset();
    void copyState(const IIRFilterer& other);
    
    void resetFilter();
    void setFilter(const IIR& filter);

    float apply(float in);
    float4_t apply(float4_t in);
#if defined(IPL_ENABLE_FLOAT8)
    float8_t IPL_FLOAT8_ATTR apply(float8_t in);
#endif
    void apply(int size, const float* in, float* out);

private:
    IIR2Filterer m_filterer2;
    IIR8Filterer m_filterer8;

    void (IIRFilterer::* m_reset)();
    void (IIRFilterer::* m_copyState)(const IIRFilterer&);
    void (IIRFilterer::* m_resetFilter)();
    void (IIRFilterer::* m_setFilter)(const IIR&);
    float (IIRFilterer::* m_applyFloat)(float);
    float4_t (IIRFilterer::* m_applyFloat4)(float4_t);
#if defined(IPL_ENABLE_FLOAT8)
    float8_t (IPL_FLOAT8_ATTR IIRFilterer::* m_applyFloat8)(float8_t);
#endif
    void (IIRFilterer::* m_applyFloatArray)(int, const float*, float*);

    void reset_iir2();
    void copyState_iir2(const IIRFilterer& other);
    void resetFilter_iir2();
    void setFilter_iir2(const IIR& filter);
    float applyFloat_iir2(float in);
    float4_t applyFloat4_iir2(float4_t in);
#if defined(IPL_ENABLE_FLOAT8)
    float8_t IPL_FLOAT8_ATTR applyFloat8_iir2(float8_t in);
#endif
    void applyFloatArray_iir2(int size, const float* in, float* out);

    void reset_switchable();
    void copyState_switchable(const IIRFilterer& other);
    void resetFilter_switchable();
    void setFilter_switchable(const IIR& filter);
    float applyFloat_switchable(float in);
    float4_t applyFloat4_switchable(float4_t in);
#if defined(IPL_ENABLE_FLOAT8)
    float8_t IPL_FLOAT8_ATTR applyFloat8_switchable(float8_t in);
#endif
    void applyFloatArray_switchable(int size, const float* in, float* out);
};

}
