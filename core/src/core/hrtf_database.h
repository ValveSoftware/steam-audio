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

#include "fft.h"
#include "hrtf_map_factory.h"
#include "vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HRTFDatabase
// --------------------------------------------------------------------------------------------------------------------

// Supported HRTF interpolation techniques.
enum class HRTFInterpolation
{
    NearestNeighbor,
    Bilinear
};

// Possible phase functions to use when using spatial blend. Spatial blend is a weighted sum of the queried HRTF
// and a "no-spatialization HRTF". The latter has a flat magnitude response, and these are the possible options for
// phase response.
enum class HRTFPhaseType
{
    None, // Flat phase response. Truly no spatialization with spatial blend = 0.
    SphereITD, // Phase response corresponding to ITD for a spherical head model.
    Full // Phase response from the queried HRTF.
};

// An HRTF database that can be queried at any given direction.
class HRTFDatabase
{
public:
    static bool sEnableDCCorrectionForPhaseInterpolation;
    static bool sEnableNyquistCorrectionForPhaseInterpolation;

    HRTFDatabase(const HRTFSettings& hrtfSettings,
                 int samplingRate,
                 int frameSize);

    int numHRIRs() const
    {
        return mHRTFMap->numHRIRs();
    }

    int numSamples() const
    {
        return mHRTFMap->numSamples();
    }

    int numSpectrumSamples() const
    {
        return mFFTAudioProcessing.numComplexSamples;
    }

    void getHRTFByIndex(int index,
                        const complex_t** hrtf) const;

    // Nearest-neighbor lookup, with optional spatial blend support.
    void nearestHRTF(const Vector3f& direction,
                     const complex_t** hrtf,
                     float spatialBlend,
                     HRTFPhaseType phaseType,
                     complex_t* const* hrtfWithBlend = nullptr,
                     int* peakDelays = nullptr);

    // Bilinear interpolated lookup, with optional spatial blend support.
    void interpolatedHRTF(const Vector3f& direction,
                          complex_t* const* hrtf,
                          float spatialBlend,
                          HRTFPhaseType phaseType,
                          int* peakDelays = nullptr);

    // Returns a precomputed Ambisonics HRTF.
    void ambisonicsHRTF(int index,
                        const complex_t** hrtf) const;

    // Saves Ambisonics HRIRs to disk.
    void saveAmbisonicsHRIRs(FILE* file);

private:
    int mSamplingRate;
    unique_ptr<IHRTFMap> mHRTFMap; // IHRTFMap containing loaded HRTF data.
    FFT mFFTInterpolation; // FFT for interpolation and min-phase conversion. #samples -> #spectrumsamples.
    FFT mFFTAudioProcessing; // FFT for audio processing. #paddedsamples (= #windowedframesamples + #samples - 1) -> #paddedspectrumsamples.
    Array<complex_t, 3> mHRTF; // HRTFs. #ears * #measurements * #paddedspectrumsamples.
    Array<int, 2> mPeakDelay; // Index of peaks in each HRIR. #ears * #measurements.
    Array<float, 3> mHRTFMagnitude; // HRTF magnitude. #ears * #measurements * #spectrumsamples.
    Array<float, 3> mHRTFPhase; // HRTF phase (unwrapped). #ears * #measurements * #spectrumsamples.
    Array<float> mInterpolatedHRTFMagnitude; // Temp. storage for interpolated HRTF magnitude. #spectrumsamples.
    Array<float> mInterpolatedHRTFPhase; // Temp. storage for interpolated HRTF phase. #spectrumsamples.
    Array<complex_t, 2> mInterpolatedHRTF; // Interpolated HRTF. #ears * #spectrumsamples.
    Array<float, 2> mInterpolatedHRIR; // Interpolated HRIR. #ears * #paddedsamples. TODO: check
    Array<complex_t, 3> mAmbisonicsHRTF; // Ambisonics HRTFs. #ears * #coefficients * #paddedspectrumsamples.
    float mReferenceLoudness; // Reference loudness of front HRIR.

    // Applies a normalization and volume scaling to the loaded HRIRs. Performs no normalization if HRTFNormType::None is selected.
    // Performs no volume scaling if volume is 0 dB.
    void applyVolumeSettings(float volume, HRTFNormType normType);
    void updateReferenceLoudness(HRTFNormType normType);

    // Applies a Fourier transform to a set of HRIRs, converting them to the corresponding HRTFs.
    void fourierTransformHRIRs(const Array<float, 3>& in,
                               Array<complex_t, 3>& out);

    // Calculates peak indices for every HRIR in a set.
    void extractPeakDelays();

    // Calculates the HRTF magnitude and phase given a set of HRIRs. Phase is unwrapped.
    void decomposeToMagnitudePhase(const Array<float, 3>& signal,
                                   Array<float, 3>& magnitude,
                                   Array<float, 3>& phase);

    // Blends up to 4 HRIRs using the given weights.
    void interpolateHRIRs(const int* indices,
                          const float* weights,
                          float spatialBlend,
                          HRTFPhaseType phaseType,
                          const Vector3f& direction = Vector3f::kZero);

    void applySpatialBlend(int numRealSamples,
                           int numComplexSamples,
                           float spatialBlend,
                           HRTFPhaseType phaseType,
                           const Vector3f& direction,
                           int ear,
                           const float* hrtfMagnitude,
                           const float* hrtfPhase,
                           float* hrtfMagnitudeBlended,
                           float* hrtfPhaseBlended);

    // Projects an HRIR set into Ambisonics.
    void precomputeAmbisonicsHRTFs(int samplingRate,
                                   int frameSize);

    // Returns the peak index for a single HRIR for a single ear.
    static int extractPeakDelay(const float* in,
                                int hrirSize);

    static void calcSphereITD(const Vector3f& direction,
                              float& leftDelay,
                              float& rightDelay);

    // Unwraps HRTF phase.
    static void unwrapPhase(Array<float, 3>& phase);

    // Wraps a phase array. Should be called as soon as an interpolated phase array is calculated.
    static void wrapPhase(Array<float>& unwrappedPhase);

    // Calculates minimum-phase versions of a set of HRIRs.
    static void convertToMinimumPhase(const Array<float, 3>& signal,
                                      Array<float, 3>& minPhaseSignal);
};

namespace Loudness
{
    constexpr float kMinDBLevel = -90.0f;
    constexpr float kGainForMinDB = 0.000032f;
    static float sMaxVolumeNormGainDB = 12.0f;

    float gainToDB(float gain);
    float dBToGain(float dB);
    float calculateRMSLoudness(int numSpectrumSamples, int samplingRate, const complex_t** hrtf);
    float calculateGainScaling(float indB, float refdB);
    void applyGainScaling(int numSamples, float** hrir, float gain);
}

}
