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

#include "hrtf_database.h"

#include "ambisonics_panning_effect.h"
#include "array_math.h"
#include "float4.h"
#include "fft.h"
#include "sh.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HRTFDatabase
// --------------------------------------------------------------------------------------------------------------------

bool HRTFDatabase::sEnableDCCorrectionForPhaseInterpolation = false;
bool HRTFDatabase::sEnableNyquistCorrectionForPhaseInterpolation = false;

HRTFDatabase::HRTFDatabase(const HRTFSettings& hrtfSettings,
                           int samplingRate,
                           int frameSize)
    : mSamplingRate(samplingRate)
    , mReferenceLoudness(0.0f)
    , mHRTFMap(HRTFMapFactory::create(hrtfSettings, samplingRate))
    , mFFTInterpolation(mHRTFMap->numSamples())
    , mFFTAudioProcessing(frameSize + (frameSize / 4) + mHRTFMap->numSamples() - 1)
    , mHRTF(IHRTFMap::kNumEars, numHRIRs(), mFFTAudioProcessing.numComplexSamples)
    , mPeakDelay(IHRTFMap::kNumEars, numHRIRs())
    , mHRTFMagnitude(IHRTFMap::kNumEars, numHRIRs(), mFFTInterpolation.numComplexSamples)
    , mHRTFPhase(IHRTFMap::kNumEars, numHRIRs(), mFFTInterpolation.numComplexSamples)
    , mInterpolatedHRTFMagnitude(mFFTInterpolation.numComplexSamples)
    , mInterpolatedHRTFPhase(mFFTInterpolation.numComplexSamples)
    , mInterpolatedHRTF(IHRTFMap::kNumEars, mFFTInterpolation.numComplexSamples)
    , mInterpolatedHRIR(IHRTFMap::kNumEars, mFFTAudioProcessing.numRealSamples)
    , mAmbisonicsHRTF(IHRTFMap::kNumEars, SphericalHarmonics::numCoeffsForOrder(IHRTFMap::kMaxAmbisonicsOrder), mFFTAudioProcessing.numComplexSamples)
{
    updateReferenceLoudness(hrtfSettings.normType);
    applyVolumeSettings(hrtfSettings.volume, hrtfSettings.normType);
    fourierTransformHRIRs(mHRTFMap->hrtfData(), mHRTF);
    extractPeakDelays();
    decomposeToMagnitudePhase(mHRTFMap->hrtfData(), mHRTFMagnitude, mHRTFPhase);

    const auto& ambisonicsHRIR = mHRTFMap->ambisonicsData();
    if (ambisonicsHRIR.totalSize() == 0)
    {
        precomputeAmbisonicsHRTFs(samplingRate, frameSize);
    }
    else
    {
        fourierTransformHRIRs(ambisonicsHRIR, mAmbisonicsHRTF);
    }
}

void HRTFDatabase::getHRTFByIndex(int index,
                                  const complex_t** hrtf) const
{
    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        hrtf[i] = mHRTF[i][index];
    }
}

void HRTFDatabase::nearestHRTF(const Vector3f& direction,
                               const complex_t** hrtf,
                               float spatialBlend,
                               HRTFPhaseType phaseType,
                               complex_t* const* hrtfWithBlend,
                               int* peakDelays)
{
    PROFILE_FUNCTION();

    auto index = mHRTFMap->nearestHRIR(direction);

    getHRTFByIndex(index, hrtf);

    if (spatialBlend < 1.0f)
    {
        auto numRealSamples = mFFTInterpolation.numRealSamples;
        auto numComplexSamples = mFFTInterpolation.numComplexSamples;

        for (auto i = 0; i < 2; ++i)
        {
            applySpatialBlend(numRealSamples, numComplexSamples, spatialBlend, phaseType, direction, i,
                              mHRTFMagnitude[i][index], mHRTFPhase[i][index],
                              mInterpolatedHRTFMagnitude.data(), mInterpolatedHRTFPhase.data());

            wrapPhase(mInterpolatedHRTFPhase);

            ArrayMath::exp(static_cast<int>(mInterpolatedHRTFMagnitude.size(0)), mInterpolatedHRTFMagnitude.data(),
                           mInterpolatedHRTFMagnitude.data());

            ArrayMath::polarToCartesian(static_cast<int>(mInterpolatedHRTFMagnitude.size(0)), mInterpolatedHRTFMagnitude.data(),
                                        mInterpolatedHRTFPhase.data(), mInterpolatedHRTF[i]);

            memset(mInterpolatedHRIR[i], 0, mInterpolatedHRIR.size(1) * sizeof(float));
            mFFTInterpolation.applyInverse(mInterpolatedHRTF[i], mInterpolatedHRIR[i]);

            mFFTAudioProcessing.applyForward(mInterpolatedHRIR[i], hrtfWithBlend[i]);
        }
    }

    if (peakDelays)
    {
        for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
        {
            peakDelays[i] = mPeakDelay[i][index];
        }
    }
}

void HRTFDatabase::interpolatedHRTF(const Vector3f& direction,
                                    complex_t* const* hrtf,
                                    float spatialBlend,
                                    HRTFPhaseType phaseType,
                                    int* peakDelays)
{
    PROFILE_FUNCTION();

    int indices[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    float weights[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    mHRTFMap->interpolatedHRIRWeights(direction, indices, weights);

    interpolateHRIRs(indices, weights, spatialBlend, phaseType, direction);

    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        memset(mInterpolatedHRIR[i], 0, mInterpolatedHRIR.size(1) * sizeof(float));
        mFFTInterpolation.applyInverse(mInterpolatedHRTF[i], mInterpolatedHRIR[i]);

        if (peakDelays)
        {
            peakDelays[i] = extractPeakDelay(mInterpolatedHRIR[i], mHRTFMap->numSamples());
        }

        mFFTAudioProcessing.applyForward(mInterpolatedHRIR[i], hrtf[i]);
    }
}

void HRTFDatabase::ambisonicsHRTF(int index,
                                  const complex_t** hrtf) const
{
    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        hrtf[i] = mAmbisonicsHRTF[i][index];
    }
}

float Loudness::gainToDB(float gain)
{
    if (gain <= kGainForMinDB)
        return kMinDBLevel;
    float dB = 20.0f * log10f(gain);
    return dB;
}

float Loudness::dBToGain(float dB)
{
    if (dB <= kMinDBLevel)
        return 0.0f;

    float gain = powf(10.0f, dB * (1.0f / 20.0f));
    return gain;
}

float Loudness::calculateRMSLoudness(int numSpectrumSamples, int samplingRate, const complex_t** hrtf)
{
    float rms = .0;

    int fqs[] = { 90, 180, 360, 720, 1440, 2880, 5760, std::numeric_limits<int>::max() };
    int numBands = sizeof(fqs) / sizeof(int);
    vector<float> fqLoudness(numBands);

    for (int i = 0; i < numBands; ++i)
    {
        fqLoudness[i] = .0f;
    }

    for (int j = 0, bandIdx = 0, count=0; j<numSpectrumSamples; ++j)
    {
        for (int i = 0; i < IHRTFMap::kNumEars; ++i, ++count)
            fqLoudness[bandIdx] += std::norm(hrtf[i][j]);

        float freq = j * samplingRate / (2.0f * static_cast<float>(numSpectrumSamples));
        if (freq > fqs[bandIdx] || j == (numSpectrumSamples-1))
        {
            if (count > 0)
                fqLoudness[bandIdx] /= count;

            count = 0;
            ++bandIdx;
        }
    }

    for (int i = 0; i < numBands; ++i)
    {
        rms += (fqLoudness[i] / numBands);
    }

    return gainToDB(sqrtf(rms));
}

float Loudness::calculateGainScaling(float indB, float refdB)
{
    return dBToGain(refdB - indB);
}

void Loudness::applyGainScaling(int numSamples, float** hrir, float gain)
{
    for (int i = 0; i < IHRTFMap::kNumEars; ++i)
        for (int j = 0; j < numSamples; ++j)
            hrir[i][j] *= gain;
}


void HRTFDatabase::updateReferenceLoudness(HRTFNormType normType)
{
    auto& hrirs = mHRTFMap->hrtfData();
    Array<complex_t, 2> hrtf(IHRTFMap::kNumEars, mFFTAudioProcessing.numComplexSamples);
    Array<float> zeroPadded(mFFTAudioProcessing.numRealSamples);
    zeroPadded.zero();

    auto index = mHRTFMap->nearestHRIR(ipl::Vector3f(.0f, .0f, -1.0f));
    vector<float*> hrirData;
    for (auto i = 0u; i < hrirs.size(0); ++i)
    {
        hrirData.push_back(hrirs[i][index]);
        memcpy(zeroPadded.data(), hrirs[i][index], numSamples() * sizeof(float));
        mFFTAudioProcessing.applyForward(zeroPadded.data(), hrtf[i]);
    }

    mReferenceLoudness = Loudness::calculateRMSLoudness(numSpectrumSamples(), mSamplingRate, (const ipl::complex_t**)hrtf.data());
}

void HRTFDatabase::applyVolumeSettings(float volume, HRTFNormType normType)
{
    if (normType == HRTFNormType::RMS)
    {
        auto& hrirs = mHRTFMap->hrtfData();
        Array<complex_t, 2> mHRTF(IHRTFMap::kNumEars, mFFTAudioProcessing.numComplexSamples);
        Array<float> zeroPadded(mFFTAudioProcessing.numRealSamples);
        zeroPadded.zero();

        for (auto j = 0u; j < hrirs.size(1); ++j)
        {
            vector<float*> hrirData;
            for (auto i = 0u; i < hrirs.size(0); ++i)
            {
                hrirData.push_back(hrirs[i][j]);
                memcpy(zeroPadded.data(), hrirs[i][j], numSamples() * sizeof(float));
                mFFTAudioProcessing.applyForward(zeroPadded.data(), mHRTF[i]);
            }

            float rmsValue = Loudness::calculateRMSLoudness(numSpectrumSamples(), mSamplingRate, (const ipl::complex_t**)mHRTF.data());
            float gain = std::min( Loudness::dBToGain(Loudness::sMaxVolumeNormGainDB), Loudness::calculateGainScaling(rmsValue, mReferenceLoudness));
            Loudness::applyGainScaling(numSamples(), hrirData.data(), gain);
        }
    }

    if (volume != 0.0f)
    {
        auto& hrirs = mHRTFMap->hrtfData();
        auto gain = Loudness::dBToGain(volume);
        ArrayMath::scale(static_cast<int>(hrirs.totalSize()), hrirs.flatData(), gain, hrirs.flatData());
    }

}

void HRTFDatabase::fourierTransformHRIRs(const Array<float, 3>& in,
                                         Array<complex_t, 3>& out)
{
    Array<float> zeroPadded(mFFTAudioProcessing.numRealSamples);
    zeroPadded.zero();

    for (auto i = 0u; i < in.size(0); ++i)
    {
        for (auto j = 0u; j < in.size(1); ++j)
        {
            // Input HRIRs are zero-padded before being transformed, so the result can be directly used for
            // convolution with audio frames.
            memcpy(zeroPadded.data(), in[i][j], numSamples() * sizeof(float));
            mFFTAudioProcessing.applyForward(zeroPadded.data(), out[i][j]);
        }
    }
}

void HRTFDatabase::extractPeakDelays()
{
    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        for (auto j = 0; j < numHRIRs(); ++j)
        {
            mPeakDelay[i][j] = extractPeakDelay(mHRTFMap->hrtfData()[i][j], mHRTFMap->numSamples());
        }
    }
}

void HRTFDatabase::decomposeToMagnitudePhase(const Array<float, 3>& signal,
                                             Array<float, 3>& magnitude,
                                             Array<float, 3>& phase)
{
    Array<float> in(mFFTInterpolation.numRealSamples);
    in.zero();

    Array<complex_t> out(mFFTInterpolation.numComplexSamples);

    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        for (auto j = 0; j < numHRIRs(); ++j)
        {
            // Input HRIRs are *not* zero-padded, because the magnitude-phase decomposition is independent of
            // the audio frame size. The magnitude-phase decomposition can potentially even be saved along with
            // the HRTF data to speed up load times.
            memcpy(in.data(), signal[i][j], numSamples() * sizeof(float));
            mFFTInterpolation.applyForward(in.data(), out.data());

            ArrayMath::magnitude(mFFTInterpolation.numComplexSamples, out.data(), magnitude[i][j]);
            ArrayMath::addConstant(mFFTInterpolation.numComplexSamples, magnitude[i][j], 1e-9f, magnitude[i][j]);
            ArrayMath::log(mFFTInterpolation.numComplexSamples, magnitude[i][j], magnitude[i][j]);

            ArrayMath::phase(mFFTInterpolation.numComplexSamples, out.data(), phase[i][j]);
        }
    }

    unwrapPhase(phase);
}

void HRTFDatabase::interpolateHRIRs(const int* indices,
                                    const float* weights,
                                    float spatialBlend,
                                    HRTFPhaseType phaseType,
                                    const Vector3f& direction)
{
    PROFILE_FUNCTION();

    auto size = mFFTInterpolation.numComplexSamples;

    auto numValidIndices = 0;
    int validIndices[8] = { 0 };
    float weightsForValidIndices[8] = { 0 };
    for (auto i = 0; i < 8; ++i)
    {
        if (indices[i] >= 0)
        {
            validIndices[numValidIndices] = indices[i];
            weightsForValidIndices[numValidIndices] = weights[i];
            ++numValidIndices;
        }
    }

    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        // Since the phase has been unwrapped, we can just linearly interpolate the magnitude and phase
        // separately.
        ArrayMath::scale(static_cast<int>(mInterpolatedHRTFMagnitude.size(0)), mHRTFMagnitude[i][validIndices[0]], weightsForValidIndices[0], mInterpolatedHRTFMagnitude.data());
        ArrayMath::scale(static_cast<int>(mInterpolatedHRTFPhase.size(0)), mHRTFPhase[i][validIndices[0]], weightsForValidIndices[0], mInterpolatedHRTFPhase.data());
        for (auto j = 1; j < numValidIndices; ++j)
        {
            ArrayMath::scaleAccumulate(static_cast<int>(mInterpolatedHRTFMagnitude.size(0)), mHRTFMagnitude[i][validIndices[j]], weightsForValidIndices[j], mInterpolatedHRTFMagnitude.data());
            ArrayMath::scaleAccumulate(static_cast<int>(mInterpolatedHRTFPhase.size(0)), mHRTFPhase[i][validIndices[j]], weightsForValidIndices[j], mInterpolatedHRTFPhase.data());
        }

        if (spatialBlend < 1.0f)
        {
            applySpatialBlend(mFFTInterpolation.numRealSamples, size, spatialBlend, phaseType, direction, i,
                              mInterpolatedHRTFMagnitude.data(), mInterpolatedHRTFPhase.data(),
                              mInterpolatedHRTFMagnitude.data(), mInterpolatedHRTFPhase.data());
        }

        // After interpolation, wrap the phase.
        wrapPhase(mInterpolatedHRTFPhase);

        ArrayMath::exp(static_cast<int>(mInterpolatedHRTFMagnitude.size(0)), mInterpolatedHRTFMagnitude.data(), mInterpolatedHRTFMagnitude.data());
        ArrayMath::polarToCartesian(static_cast<int>(mInterpolatedHRTFMagnitude.size(0)), mInterpolatedHRTFMagnitude.data(), mInterpolatedHRTFPhase.data(), mInterpolatedHRTF[i]);
    }
}

void HRTFDatabase::applySpatialBlend(int numRealSamples,
                                     int numComplexSamples,
                                     float spatialBlend,
                                     HRTFPhaseType phaseType,
                                     const Vector3f& direction,
                                     int ear,
                                     const float* hrtfMagnitude,
                                     const float* hrtfPhase,
                                     float* hrtfMagnitudeBlended,
                                     float* hrtfPhaseBlended)
{
    auto leftDelay = 0.0f;
    auto rightDelay = 0.0f;
    if (phaseType == HRTFPhaseType::SphereITD)
    {
        calcSphereITD(direction, leftDelay, rightDelay);
    }

    auto maxMagnitude = 0.0f;
    for (auto k = 0; k < numComplexSamples; ++k)
    {
        if (hrtfMagnitude[k] > maxMagnitude)
        {
            maxMagnitude = hrtfMagnitude[k];
        }
    }

    for (auto k = 0; k < numComplexSamples; ++k)
    {
        hrtfMagnitudeBlended[k] = (1.0f - spatialBlend) * 0.0f + spatialBlend * hrtfMagnitude[k];
    }

    if (phaseType == HRTFPhaseType::None)
    {
        for (auto k = 0; k < numComplexSamples; ++k)
        {
            hrtfPhaseBlended[k] = spatialBlend * hrtfPhase[k];
        }

        if (sEnableDCCorrectionForPhaseInterpolation)
        {
            auto dcPhaseTarget = 0.0f;
            if (hrtfPhaseBlended[0] > Math::kPi / 2.0f)
            {
                dcPhaseTarget = Math::kPi;
            }
            else if (hrtfPhaseBlended[0] < Math::kPi / 2.0f)
            {
                dcPhaseTarget = -Math::kPi;
            }

            auto dcPhaseDelta = dcPhaseTarget - hrtfPhaseBlended[0];

            for (auto k = 0; k < numComplexSamples; ++k)
            {
                hrtfPhaseBlended[k] += dcPhaseDelta;
            }
        }

        if (sEnableNyquistCorrectionForPhaseInterpolation)
        {
            hrtfPhaseBlended[numComplexSamples - 1] = roundf(hrtfPhaseBlended[numComplexSamples - 1] / Math::kPi) * Math::kPi;
        }
    }
    else if (phaseType == HRTFPhaseType::SphereITD)
    {
        auto itdDelay = (ear == 0) ? leftDelay : rightDelay;
        auto itdDelayInSamples = floorf(itdDelay * mSamplingRate);

        for (auto k = 0; k < numComplexSamples; ++k)
        {
            auto w = k * (2.0f * Math::kPi) / numRealSamples;
            auto itdPhase = -w * itdDelayInSamples;

            hrtfPhaseBlended[k] = (1.0f - spatialBlend) * itdPhase + spatialBlend * hrtfPhase[k];
        }
    }
    else
    {
        for (auto k = 0; k < numComplexSamples; ++k)
        {
            hrtfPhaseBlended[k] = hrtfPhase[k];
        }
    }
}

void HRTFDatabase::precomputeAmbisonicsHRTFs(int samplingRate,
                                             int frameSize)
{
    const auto numCoefficients = SphericalHarmonics::numCoeffsForOrder(IHRTFMap::kMaxAmbisonicsOrder);

    const auto numSpeakers = AmbisonicsPanningEffect::kNumVirtualSpeakers;
    const auto virtualSpeakers = AmbisonicsPanningEffect::kVirtualSpeakers;

    Array<float, 3> minPhaseHRIR(IHRTFMap::kNumEars, numHRIRs(), numSamples());
    convertToMinimumPhase(mHRTFMap->hrtfData(), minPhaseHRIR);

    Array<float, 3> minPhaseHRTFMagnitude(IHRTFMap::kNumEars, numHRIRs(), mFFTInterpolation.numComplexSamples);
    Array<float, 3> minPhaseHRTFPhase(IHRTFMap::kNumEars, numHRIRs(), mFFTInterpolation.numComplexSamples);
    decomposeToMagnitudePhase(minPhaseHRIR, minPhaseHRTFMagnitude, minPhaseHRTFPhase);

    mAmbisonicsHRTF.zero();

    Array<complex_t> tempInterpolatedHRTF(mFFTAudioProcessing.numComplexSamples);

    for (auto l = 0, index = 0; l <= IHRTFMap::kMaxAmbisonicsOrder; ++l)
    {
        for (auto m = -l; m <= l; ++m, ++index)
        {
            for (auto i = 0; i < numSpeakers; ++i)
            {
                auto weight = ((4.0f * Math::kPi) / numSpeakers) * SphericalHarmonics::evaluate(l, m, virtualSpeakers[i]);

                int indices[8];
                float weights[8];
                mHRTFMap->interpolatedHRIRWeights(virtualSpeakers[i], indices, weights);

                // We can just blend the (smaller) interpolatedHRTF for each virtual speaker, IFFT it once, and
                // then FFT it once with zero-padding. This will reduce the number of IFFT/FFTs required during the SH
                // projection step by a factor of #virtualspeakers.
                interpolateHRIRs(indices, weights, 1.0f, HRTFPhaseType::None);

                for (auto j = 0; j < IHRTFMap::kNumEars; ++j)
                {
                    memcpy(tempInterpolatedHRTF.data(), mInterpolatedHRTF[j], mInterpolatedHRTF.size(1) * sizeof(complex_t));
                    ArrayMath::scale(mFFTAudioProcessing.numComplexSamples, tempInterpolatedHRTF.data(), weight, tempInterpolatedHRTF.data());
                    ArrayMath::add(mFFTAudioProcessing.numComplexSamples, mAmbisonicsHRTF[j][index], tempInterpolatedHRTF.data(), mAmbisonicsHRTF[j][index]);
                }
            }

            for (auto j = 0; j < IHRTFMap::kNumEars; ++j)
            {
                mFFTInterpolation.applyInverse(mAmbisonicsHRTF[j][index], mInterpolatedHRIR[j]);
                memset(mInterpolatedHRIR[j] + numSamples(), 0, (mFFTInterpolation.numRealSamples - numSamples()) * sizeof(float));
                mFFTAudioProcessing.applyForward(mInterpolatedHRIR[j], mAmbisonicsHRTF[j][index]);
            }
        }
    }
}

void HRTFDatabase::saveAmbisonicsHRIRs(FILE* file)
{
    if (file)
    {
        auto order = IHRTFMap::kMaxAmbisonicsOrder;
        fwrite(&order, sizeof(int32_t), 1, file);

        fwrite(&mSamplingRate, sizeof(int32_t), 1, file);

        auto numSamples = mHRTFMap->numSamples();
        fwrite(&numSamples, sizeof(int32_t), 1, file);

        vector<float> ambisonicsHRIR(mFFTAudioProcessing.numRealSamples);

        for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
        {
            for (auto j = 0u; j < mAmbisonicsHRTF.size(1); ++j)
            {
                mFFTAudioProcessing.applyInverse(mAmbisonicsHRTF[i][j], ambisonicsHRIR.data());
                fwrite(ambisonicsHRIR.data(), sizeof(float), numSamples, file);
            }
        }
    }
}

int HRTFDatabase::extractPeakDelay(const float* in,
                                   int hrirSize)
{
    auto peakValue = 0.0f;
    auto peakIndex = 0;

    for (auto i = 0; i < hrirSize; ++i)
    {
        if (in[i] > peakValue)
        {
            peakValue = in[i];
            peakIndex = i;
        }
    }

    return peakIndex;
}

void HRTFDatabase::calcSphereITD(const Vector3f& direction,
                                 float& leftDelay,
                                 float& rightDelay)
{
    const auto kHeadSize = 0.09f;
    const auto kMinDistance = 1.0f - (kHeadSize / 2.0f);
    const auto kMaxDistance = 1.0f + (kHeadSize / 2.0f);
    const auto kSpeedOfSound = 340.0f;
    const auto kMaxITD = kHeadSize / kSpeedOfSound;

    const auto kLeftEar = -0.5f * kHeadSize * Vector3f::kXAxis;
    const auto kRightEar = 0.5f * kHeadSize * Vector3f::kXAxis;

    auto leftDistance = (direction - kLeftEar).length();
    auto rightDistance = (direction - kRightEar).length();

    auto leftDistanceFraction = (leftDistance - kMinDistance) / (kMaxDistance - kMinDistance);
    auto rightDistanceFraction = (rightDistance - kMinDistance) / (kMaxDistance - kMinDistance);

    leftDelay = leftDistanceFraction * kMaxITD;
    rightDelay = rightDistanceFraction * kMaxITD;
}

void HRTFDatabase::unwrapPhase(Array<float, 3>& phase)
{
    Array<float> interpolatedPhaseDelta(phase.size(2));
    Array<float> interpolatedPhaseEquivalentDelta(phase.size(2));
    Array<float> interpolatedPhaseCorrection(phase.size(2));
    Array<float> interpolatedPhaseCumulativeDelta(phase.size(2));

    auto delta = interpolatedPhaseDelta.data();
    auto deltaEquivalent = interpolatedPhaseEquivalentDelta.data();
    auto deltaCorrection = interpolatedPhaseCorrection.data();
    auto deltaCumulative = interpolatedPhaseCumulativeDelta.data();
    auto deltaCutoff = Math::kPi;

    for (auto i = 0u; i < phase.size(0); ++i)
    {
        for (auto j = 0u; j < phase.size(1); ++j)
        {
            auto data = phase[i][j];
            auto dataLength = static_cast<int>(phase.size(2));

            // Record phase variation.
            for (auto k = 0; k < dataLength - 1; ++k)
            {
                delta[k] = data[k + 1] - data[k];
            }

            // Equivalent phase variation.
            for (auto k = 0; k < dataLength - 1; ++k)
            {
                deltaEquivalent[k] = (delta[k] + Math::kPi)
                    - floorf((delta[k] + Math::kPi) / (2.0f * Math::kPi)) * (2.0f * Math::kPi) - Math::kPi;
            }

            // Preserve variation sign for +PI vs. -PI.
            for (auto k = 0; k < dataLength - 1; ++k)
            {
                if ((deltaEquivalent[k] == -Math::kPi) && (delta[k] > .0f))
                {
                    deltaEquivalent[k] = Math::kPi;
                }
            }

            // Incremental phase correction.
            for (auto k = 0; k < dataLength - 1; ++k)
            {
                deltaCorrection[k] = deltaEquivalent[k] - delta[k];
            }

            // Ignore correction when incremental variation is smaller than cutoff
            for (auto k = 0; k < dataLength - 1; ++k)
            {
                if (fabsf(delta[k]) < deltaCutoff)
                {
                    deltaCorrection[k] = 0;
                }
            }

            // Find cumulative sum of deltas.
            deltaCumulative[0] = deltaCorrection[0];
            for (auto k = 1; k < dataLength - 1; ++k)
            {
                deltaCumulative[k] = deltaCumulative[k - 1] + deltaCorrection[k];
            }

            // Add cumulative correction to produce smoothed phase values.
            for (auto k = 1; k < dataLength; ++k)
            {
                data[k] += deltaCumulative[k - 1];
            }
        }
    }
}

void HRTFDatabase::wrapPhase(Array<float>& unwrappedPhase)
{
    auto samples = unwrappedPhase.data();
    auto size = unwrappedPhase.size(0);
    auto simdSize = size & ~3;

    auto twoPi = float4::set1(2.0f * Math::kPi);
    auto twoPiInverse = float4::set1(1.0f / (2.0f * Math::kPi));

    for (auto i = 0u; i < simdSize; i += 4)
    {
        auto phase = float4::load(samples + i);
        auto phaseWrapped = float4::sub(phase, float4::mul(twoPi, float4::round(float4::mul(phase, twoPiInverse))));
        float4::store(samples + i, phaseWrapped);
    }

    for (auto i = simdSize; i < size; ++i)
    {
        samples[i] -= 2 * Math::kPi * roundf(samples[i] / (2 * Math::kPi));
    }
}

// Based on https://ccrma.stanford.edu/~jos/fp/Matlab_listing_mps_m.html
void HRTFDatabase::convertToMinimumPhase(const Array<float, 3>& signal,
                                         Array<float, 3>& minPhaseSignal)
{
    const auto kMinPhaseMagnitudeThreshold = 1e-5f;

    auto numHRIRs = static_cast<int>(signal.size(1));
    auto numSamples = static_cast<int>(signal.size(2));

    FFT fft(numSamples, FFTDomain::Complex);

    Array<complex_t> signalComplex(fft.numRealSamples);
    Array<complex_t> spectrum(fft.numComplexSamples);
    Array<float> magnitude(fft.numComplexSamples);
    Array<float> logMagnitude(fft.numComplexSamples);
    Array<complex_t> logMagnitudeComplex(fft.numComplexSamples);
    Array<complex_t> ifftLogMagnitude(fft.numRealSamples);
    Array<complex_t> foldIFFTLogMagnitude(fft.numRealSamples);
    Array<complex_t> fftFoldIFFTLogMagnitude(fft.numComplexSamples);
    Array<complex_t> expFFTFoldIFFTLogMagnitude(fft.numComplexSamples);
    Array<complex_t> minPhaseSignalComplex(fft.numComplexSamples);

    memset(signalComplex.data(), 0, signalComplex.size(0) * sizeof(complex_t));

    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        for (auto j = 0; j < numHRIRs; ++j)
        {
            for (auto k = 0; k < numSamples; ++k)
            {
                signalComplex[k] = signal[i][j][k];
            }

            fft.applyForward(signalComplex.data(), spectrum.data());

            auto maxMagnitude = 0.0f;
            ArrayMath::magnitude(static_cast<int>(spectrum.size(0)), spectrum.data(), magnitude.data());
            ArrayMath::max(static_cast<int>(magnitude.size(0)), magnitude.data(), maxMagnitude);
            ArrayMath::threshold(static_cast<int>(magnitude.size(0)), magnitude.data(), kMinPhaseMagnitudeThreshold * maxMagnitude, magnitude.data());
            ArrayMath::log(static_cast<int>(magnitude.size(0)), magnitude.data(), logMagnitude.data());

            for (auto k = 0u; k < spectrum.size(0); ++k)
            {
                logMagnitudeComplex[k] = logMagnitude[k];
            }

            fft.applyInverse(logMagnitudeComplex.data(), ifftLogMagnitude.data());

            auto numSamples = fft.numRealSamples;

            auto halfway = (numSamples + 1) / 2;
            if (numSamples % 2 == 1)
            {
                for (auto k = 0; k < halfway; ++k)
                {
                    foldIFFTLogMagnitude[k] = ifftLogMagnitude[k];
                }
                for (auto k = 1; k < halfway; ++k)
                {
                    foldIFFTLogMagnitude[k] += ifftLogMagnitude[numSamples - k];
                }
            }
            else
            {
                for (auto k = 0; k <= halfway; ++k)
                {
                    foldIFFTLogMagnitude[k] = ifftLogMagnitude[k];
                }
                for (auto k = 1; k < halfway; ++k)
                {
                    foldIFFTLogMagnitude[k] += ifftLogMagnitude[numSamples - k];
                }
            }

            fft.applyForward(foldIFFTLogMagnitude.data(), fftFoldIFFTLogMagnitude.data());
            ArrayMath::exp(static_cast<int>(fftFoldIFFTLogMagnitude.size(0)), fftFoldIFFTLogMagnitude.data(), expFFTFoldIFFTLogMagnitude.data());
            fft.applyInverse(expFFTFoldIFFTLogMagnitude.data(), minPhaseSignalComplex.data());

            for (auto k = 0u; k < minPhaseSignal.size(2); ++k)
            {
                minPhaseSignal[i][j][k] = minPhaseSignalComplex[k].real();
            }
        }
    }
}

}
