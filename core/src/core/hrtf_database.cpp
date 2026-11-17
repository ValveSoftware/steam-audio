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
    , mAmbisonicsHRTF(IHRTFMap::kNumEars, numAmbisonicsHRTFs(), mFFTAudioProcessing.numComplexSamples)
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

void HRTFDatabase::ambisonicsHRTF(int order,
                                  int index,
                                  const complex_t** hrtf) const
{
    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
    {
        hrtf[i] = mAmbisonicsHRTF[i][ambisonicsHRTFOffset(order) + index];
    }
}

void HRTFDatabase::ambisonicsHRTF(int index,
                                  const complex_t** hrtf) const
{
    ambisonicsHRTF(IHRTFMap::kMaxAmbisonicsOrder, index, hrtf);
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

// The binaural Ambisonics decoding filters are designed per order N (0..3), since the best order-N filters are not
// a truncation of the order-3 ones. For every frequency bin and ear, the filters h_SH (one per Ambisonics channel)
// are fitted so that the decode y_N(d)^T h_SH approximates the HRTF in every direction d of a dense design grid:
//
// 1. Magnitude least squares (MagLS). Below a cutoff frequency, an ordinary least-squares fit of the complex HRTF,
//    which keeps the interaural phase (ITD). At and above it, only the magnitude is fitted: the phase of the
//    target in each direction is taken from the decode of the previous frequency bin, then least squares. A
//    low-order fit of the measured phase cancels at high frequencies (the HRTF phase changes faster with direction
//    than order N can follow), which made the decode lose up to ~14 dB of high frequencies.
// 2. Diffuse-field covariance constraint. A 2x2 matrix M mixes the left and right filters so that the 2x2
//    covariance matrix of the decode over all directions (diagonal: diffuse-field response of each ear;
//    off-diagonal: interaural cross-spectrum) equals that of the HRTF. M is the solution closest to identity.
//    For order 0 (a single channel, which cannot be decorrelated), only the diffuse-field response is matched.
//
// The filters are complete decoders: no max-rE weighting is applied, neither here nor at run time. In our tests,
// max-rE (applied before the covariance constraint) lowered the order-3 ITD from ~105 % to ~88 % of the HRTF's and
// raised the ILD error; without the constraint it also cost up to 5 dB of diffuse-field response.
//
// References: C. Schoerkhuber, M. Zaunschirm, R. Hoeldrich, "Binaural rendering of Ambisonic signals via magnitude
// least squares", DAGA 2018. M. Zaunschirm, C. Schoerkhuber, R. Hoeldrich, "Binaural rendering of Ambisonic
// signals by head-related impulse response time alignment and a diffuseness constraint", JASA 143(6), 2018.
// F. Zotter, M. Frank, "Ambisonics", Springer 2019, sections 4.11.2-4.11.3 and appendix A.5. J. Vilkamo,
// T. Baeckstroem, A. Kuntz, "Optimized covariance domain framework for time-frequency processing of spatial
// audio", JAES 61(6), 2013.

namespace {

using cplx = std::complex<double>;

// Below this frequency, the filters are an ordinary least-squares fit (keeps the ITD); at and above it, MagLS.
// 1.5 kHz is the cutoff of the MagLS decoder of the Spatial Audio Framework (ambi_bin); 2 kHz and order-dependent
// cutoffs (N c / (2 pi r)) gave larger ILD and per-direction level errors in our tests.
const double kMagLSCutoffFrequency = 1500.0;

// Number of directions of the design grid.
const int kNumDesignDirections = 480;

struct Matrix2c
{
    cplx m[2][2];
};

Matrix2c mul(const Matrix2c& a, const Matrix2c& b)
{
    Matrix2c c{};
    for (auto i = 0; i < 2; ++i)
        for (auto j = 0; j < 2; ++j)
            c.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j];
    return c;
}

Matrix2c adjoint(const Matrix2c& a)
{
    Matrix2c c{};
    for (auto i = 0; i < 2; ++i)
        for (auto j = 0; j < 2; ++j)
            c.m[i][j] = std::conj(a.m[j][i]);
    return c;
}

Matrix2c inverse(const Matrix2c& a)
{
    auto det = a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0];
    Matrix2c c{};
    c.m[0][0] = a.m[1][1] / det;
    c.m[0][1] = -a.m[0][1] / det;
    c.m[1][0] = -a.m[1][0] / det;
    c.m[1][1] = a.m[0][0] / det;
    return c;
}

// Upper-triangular X such that R = X^H X, for a Hermitian positive-definite R.
Matrix2c choleskyUpper(const Matrix2c& r)
{
    Matrix2c x{};
    auto x00 = std::sqrt(std::max(r.m[0][0].real(), 1e-30));
    x.m[0][0] = x00;
    x.m[0][1] = r.m[0][1] / x00;
    x.m[1][1] = std::sqrt(std::max(r.m[1][1].real() - std::norm(x.m[0][1]), 1e-30));
    return x;
}

// Inverse square root of a Hermitian positive-definite 2x2 matrix C:
// sqrt(C) = (C + s I) / t, with s = sqrt(det C) and t = sqrt(tr C + 2 s).
Matrix2c inverseSqrt(const Matrix2c& c)
{
    auto s = std::sqrt(std::max((c.m[0][0] * c.m[1][1] - c.m[0][1] * c.m[1][0]).real(), 0.0));
    auto t = std::sqrt(std::max(c.m[0][0].real() + c.m[1][1].real() + 2.0 * s, 1e-30));
    Matrix2c root = c;
    root.m[0][0] += s;
    root.m[1][1] += s;
    for (auto i = 0; i < 2; ++i)
        for (auto j = 0; j < 2; ++j)
            root.m[i][j] /= t;
    return inverse(root);
}

// Mixing matrix M for row vectors [left, right]: covariance R_hat becomes M^H R_hat M = R, with M as close to
// identity as possible (maximum Re tr(R_hat M)). With R = X^H X and R_hat = Xh^H Xh (Cholesky), any
// M = Xh^-1 P X with P unitary meets the constraint; the trace is Re tr(P X Xh^H), which is largest when P is the
// adjoint of the unitary polar factor of B = X Xh^H, i.e. P = (B (B^H B)^-1/2)^H.
Matrix2c covarianceMixingMatrix(const Matrix2c& rHat,
                                const Matrix2c& r)
{
    auto x = choleskyUpper(r);
    auto xHat = choleskyUpper(rHat);
    auto b = mul(x, adjoint(xHat));
    auto polar = mul(b, inverseSqrt(mul(adjoint(b), b)));
    return mul(mul(inverse(xHat), adjoint(polar)), x);
}

// Solves A X = B for a symmetric positive-definite n x n matrix A (row-major); B is n x numRhs, row-major, and is
// overwritten with X.
void choleskySolve(int n,
                   vector<double> a,
                   vector<double>& b,
                   int numRhs)
{
    for (auto j = 0; j < n; ++j)
    {
        auto d = a[j * n + j];
        for (auto k = 0; k < j; ++k)
            d -= a[j * n + k] * a[j * n + k];
        d = std::sqrt(std::max(d, 1e-30));
        a[j * n + j] = d;
        for (auto i = j + 1; i < n; ++i)
        {
            auto v = a[i * n + j];
            for (auto k = 0; k < j; ++k)
                v -= a[i * n + k] * a[j * n + k];
            a[i * n + j] = v / d;
        }
    }

    for (auto c = 0; c < numRhs; ++c)
    {
        for (auto i = 0; i < n; ++i)
        {
            auto v = b[i * numRhs + c];
            for (auto k = 0; k < i; ++k)
                v -= a[i * n + k] * b[k * numRhs + c];
            b[i * numRhs + c] = v / a[i * n + i];
        }
        for (auto i = n - 1; i >= 0; --i)
        {
            auto v = b[i * numRhs + c];
            for (auto k = i + 1; k < n; ++k)
                v -= a[k * n + i] * b[k * numRhs + c];
            b[i * numRhs + c] = v / a[i * n + i];
        }
    }
}

}

int HRTFDatabase::ambisonicsHRTFOffset(int order)
{
    // sum_{n < order} (n + 1)^2
    return (order * (order + 1) * (2 * order + 1)) / 6;
}

int HRTFDatabase::numAmbisonicsHRTFs()
{
    return ambisonicsHRTFOffset(IHRTFMap::kMaxAmbisonicsOrder + 1);
}

void HRTFDatabase::precomputeAmbisonicsHRTFs(int samplingRate,
                                             int frameSize)
{
    const auto kMaxOrder = IHRTFMap::kMaxAmbisonicsOrder;
    const auto kMaxCoeffs = SphericalHarmonics::numCoeffsForOrder(kMaxOrder);
    const auto numBins = mFFTInterpolation.numComplexSamples;
    const auto fftSize = mFFTInterpolation.numRealSamples;
    const auto numPoints = kNumDesignDirections;
    const auto pointWeight = 4.0 * Math::kPi / numPoints;

    // Design grid: a Fibonacci (golden angle) spiral, which is nearly uniform and deterministic.
    vector<Vector3f> directions(numPoints);
    const auto goldenAngle = Math::kPi * (3.0 - sqrt(5.0));
    for (auto q = 0; q < numPoints; ++q)
    {
        auto y = 1.0 - 2.0 * (q + 0.5) / numPoints;
        auto r = sqrt(std::max(0.0, 1.0 - y * y));
        directions[q] = Vector3f(static_cast<float>(r * cos(goldenAngle * q)), static_cast<float>(y),
                                 static_cast<float>(r * sin(goldenAngle * q)));
    }

    // Common delay of the HRIRs, in samples. It is removed before the design and restored after, so that the phase
    // continued above the cutoff keeps the high frequencies aligned in time with the low frequencies.
    auto meanPeakDelay = 0.0;
    for (auto i = 0; i < IHRTFMap::kNumEars; ++i)
        for (auto j = 0; j < numHRIRs(); ++j)
            meanPeakDelay += mPeakDelay[i][j];
    meanPeakDelay /= (IHRTFMap::kNumEars * numHRIRs());
    const auto bulkDelay = floor(meanPeakDelay + 0.5);

    // Target: the HRTF in each design direction, interpolated the same way as for direct rendering.
    // target[(ear * numPoints + q) * numBins + k]
    vector<cplx> target(IHRTFMap::kNumEars * numPoints * numBins);
    for (auto q = 0; q < numPoints; ++q)
    {
        int indices[8];
        float weights[8];
        mHRTFMap->interpolatedHRIRWeights(directions[q], indices, weights);
        interpolateHRIRs(indices, weights, 1.0f, HRTFPhaseType::None);

        for (auto e = 0; e < IHRTFMap::kNumEars; ++e)
        {
            for (auto k = 0; k < numBins; ++k)
            {
                auto delay = std::polar(1.0, 2.0 * Math::kPi * k * bulkDelay / fftSize);
                target[(e * numPoints + q) * numBins + k] = cplx(mInterpolatedHRTF[e][k]) * delay;
            }
        }
    }

    // Diffuse-field covariance of the HRTF, per bin: R_ij = sum_q w conj(H_i(q)) H_j(q).
    vector<Matrix2c> targetCovariance(numBins);
    for (auto k = 0; k < numBins; ++k)
    {
        Matrix2c r{};
        for (auto q = 0; q < numPoints; ++q)
        {
            cplx h[2] = { target[(0 * numPoints + q) * numBins + k], target[(1 * numPoints + q) * numBins + k] };
            for (auto i = 0; i < 2; ++i)
                for (auto j = 0; j < 2; ++j)
                    r.m[i][j] += pointWeight * std::conj(h[i]) * h[j];
        }
        targetCovariance[k] = r;
    }

    // Real spherical harmonics on the design grid. Y[q * kMaxCoeffs + c]
    vector<double> Y(numPoints * kMaxCoeffs);
    for (auto q = 0; q < numPoints; ++q)
        for (auto l = 0, c = 0; l <= kMaxOrder; ++l)
            for (auto m = -l; m <= l; ++m, ++c)
                Y[q * kMaxCoeffs + c] = SphericalHarmonics::evaluate(l, m, directions[q]);

    mAmbisonicsHRTF.zero();

    vector<cplx> coeffs(numBins * IHRTFMap::kNumEars * kMaxCoeffs); // [(k * kNumEars + e) * kMaxCoeffs + c]
    vector<cplx> fit(numPoints);
    vector<cplx> decoded(IHRTFMap::kNumEars * numPoints);
    Array<complex_t> spectrum(numBins);

    for (auto order = 0; order <= kMaxOrder; ++order)
    {
        const auto numCoeffs = SphericalHarmonics::numCoeffsForOrder(order);

        // Least-squares projector P = (Y^T W Y)^-1 Y^T W, numCoeffs x numPoints.
        vector<double> gram(numCoeffs * numCoeffs, 0.0);
        for (auto i = 0; i < numCoeffs; ++i)
            for (auto j = 0; j < numCoeffs; ++j)
                for (auto q = 0; q < numPoints; ++q)
                    gram[i * numCoeffs + j] += pointWeight * Y[q * kMaxCoeffs + i] * Y[q * kMaxCoeffs + j];

        vector<double> projector(numCoeffs * numPoints);
        for (auto i = 0; i < numCoeffs; ++i)
            for (auto q = 0; q < numPoints; ++q)
                projector[i * numPoints + q] = pointWeight * Y[q * kMaxCoeffs + i];

        choleskySolve(numCoeffs, gram, projector, numPoints);

        const auto cutoffBin = static_cast<int>(ceil(kMagLSCutoffFrequency * fftSize / samplingRate));

        auto decode = [&](int k, int e, int q)
        {
            auto value = cplx(0.0, 0.0);
            for (auto c = 0; c < numCoeffs; ++c)
                value += Y[q * kMaxCoeffs + c] * coeffs[(k * IHRTFMap::kNumEars + e) * kMaxCoeffs + c];
            return value;
        };

        // 1. MagLS.
        for (auto k = 0; k < numBins; ++k)
        {
            for (auto e = 0; e < IHRTFMap::kNumEars; ++e)
            {
                for (auto q = 0; q < numPoints; ++q)
                {
                    auto h = target[(e * numPoints + q) * numBins + k];
                    if (k > 0 && k >= cutoffBin)
                    {
                        auto previous = decode(k - 1, e, q);
                        auto magnitude = std::abs(previous);
                        h = (magnitude > 0.0) ? std::abs(h) * (previous / magnitude) : cplx(std::abs(h), 0.0);
                    }
                    fit[q] = h;
                }

                for (auto c = 0; c < numCoeffs; ++c)
                {
                    auto value = cplx(0.0, 0.0);
                    for (auto q = 0; q < numPoints; ++q)
                        value += projector[c * numPoints + q] * fit[q];
                    coeffs[(k * IHRTFMap::kNumEars + e) * kMaxCoeffs + c] = value;
                }
            }
        }

        // 2. Diffuse-field covariance constraint.
        {
            for (auto k = 0; k < numBins; ++k)
            {
                Matrix2c rHat{};
                for (auto q = 0; q < numPoints; ++q)
                {
                    cplx h[2] = { decode(k, 0, q), decode(k, 1, q) };
                    for (auto i = 0; i < 2; ++i)
                        for (auto j = 0; j < 2; ++j)
                            rHat.m[i][j] += pointWeight * std::conj(h[i]) * h[j];
                }

                Matrix2c mixing{};
                if (order == 0)
                {
                    // A single channel cannot be decorrelated: only match the diffuse-field response of each ear.
                    for (auto i = 0; i < 2; ++i)
                        mixing.m[i][i] = sqrt(targetCovariance[k].m[i][i].real() / std::max(rHat.m[i][i].real(), 1e-30));
                }
                else
                {
                    // Small diagonal loading keeps the inverse bounded where the decode is almost fully coherent.
                    auto loading = 1e-4 * (rHat.m[0][0].real() + rHat.m[1][1].real());
                    rHat.m[0][0] += loading;
                    rHat.m[1][1] += loading;
                    mixing = covarianceMixingMatrix(rHat, targetCovariance[k]);
                }

                for (auto c = 0; c < numCoeffs; ++c)
                {
                    auto left = coeffs[(k * IHRTFMap::kNumEars + 0) * kMaxCoeffs + c];
                    auto right = coeffs[(k * IHRTFMap::kNumEars + 1) * kMaxCoeffs + c];
                    coeffs[(k * IHRTFMap::kNumEars + 0) * kMaxCoeffs + c] = left * mixing.m[0][0] + right * mixing.m[1][0];
                    coeffs[(k * IHRTFMap::kNumEars + 1) * kMaxCoeffs + c] = left * mixing.m[0][1] + right * mixing.m[1][1];
                }
            }
        }

        // Restore the common delay, then convert to filters of the same length as the HRIRs.
        for (auto e = 0; e < IHRTFMap::kNumEars; ++e)
        {
            for (auto c = 0; c < numCoeffs; ++c)
            {
                for (auto k = 0; k < numBins; ++k)
                {
                    auto value = coeffs[(k * IHRTFMap::kNumEars + e) * kMaxCoeffs + c] *
                                 std::polar(1.0, -2.0 * Math::kPi * k * bulkDelay / fftSize);
                    if (k == 0 || k == numBins - 1)
                        value = cplx(value.real(), 0.0);
                    spectrum[k] = complex_t(static_cast<float>(value.real()), static_cast<float>(value.imag()));
                }

                auto index = ambisonicsHRTFOffset(order) + c;
                memset(mInterpolatedHRIR[e], 0, mInterpolatedHRIR.size(1) * sizeof(float));
                mFFTInterpolation.applyInverse(spectrum.data(), mInterpolatedHRIR[e]);
                memset(mInterpolatedHRIR[e] + numSamples(), 0, (mFFTAudioProcessing.numRealSamples - numSamples()) * sizeof(float));
                mFFTAudioProcessing.applyForward(mInterpolatedHRIR[e], mAmbisonicsHRTF[e][index]);
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

        // One filter set per order (0..order), see precomputeAmbisonicsHRTFs.
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
