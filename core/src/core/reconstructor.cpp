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

#include "reconstructor.h"

#include <cstring>
#include <functional>
#include <random>

#include "array_math.h"
#include "log.h"
#include "math_functions.h"
#include "profiler.h"
#include "propagation_medium.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IReconstructor
// --------------------------------------------------------------------------------------------------------------------

const float IReconstructor::kEnergyThreshold = 1e-7f;
const float IReconstructor::kMinVariance = 1e-5f;


// --------------------------------------------------------------------------------------------------------------------
// Reconstructor
// --------------------------------------------------------------------------------------------------------------------

Reconstructor::Reconstructor(float maxDuration,
                             int maxOrder,
                             int samplingRate)
    : mMaxDuration(maxDuration)
    , mMaxOrder(maxOrder)
    , mSamplingRate(samplingRate)
    , mWhiteNoise(Bands::kNumBands, static_cast<int>(ceilf(maxDuration * samplingRate)))
    , mBandIRs(Bands::kNumBands, static_cast<int>(ceilf(maxDuration * samplingRate)))
    , mFilters(Bands::kNumBands)
{
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        std::default_random_engine randomGenerator;
        std::uniform_real_distribution<float> uniformDistribution(-1.0f, 1.0f);
        auto uniformRandom = std::bind(uniformDistribution, randomGenerator);

        for (auto j = 0u; j < mWhiteNoise.size(1); ++j)
        {
            mWhiteNoise[i][j] = uniformRandom();
        }
    }

    mFilters[0].setFilter(IIR::lowPass(Bands::kHighCutoffFrequencies[0], samplingRate));
    for (auto i = 1; i < Bands::kNumBands - 1; ++i)
    {
        mFilters[i].setFilter(IIR::bandPass(Bands::kLowCutoffFrequencies[i], Bands::kHighCutoffFrequencies[i], samplingRate));
    }
    mFilters[Bands::kNumBands - 1].setFilter(IIR::highPass(Bands::kLowCutoffFrequencies[Bands::kNumBands - 1], samplingRate));
}

void Reconstructor::reconstruct(int numIRs,
                                const EnergyField* const* energyFields,
                                const float* const* distanceAttenuationCorrectionCurves,
                                const AirAbsorptionModel* airAbsorptionModels,
                                ImpulseResponse* const* impulseResponses,
                                ReconstructionType type,
                                float duration,
                                int order)
{
    PROFILE_FUNCTION();

    for (auto i = 0; i < numIRs; ++i)
    {
        const auto& energyField = *energyFields[i];
        auto& impulseResponse = *impulseResponses[i];

        auto numChannels = SphericalHarmonics::numCoeffsForOrder(order);
        auto numSamples = static_cast<int>(ceilf(duration * mSamplingRate));

        numChannels = std::min({ energyField.numChannels(), impulseResponse.numChannels(), numChannels });
        auto numSamplesPerBin = static_cast<int>(ceilf(EnergyField::kBinDuration * mSamplingRate));
        numSamples = std::min({ impulseResponse.numSamples(), numSamples });
        auto numBins = std::min({ energyField.numBins(), static_cast<int>(ceilf(static_cast<float>(numSamples) / static_cast<float>(numSamplesPerBin))) });

        impulseResponses[i]->reset();

        for (auto iChannel = 0; iChannel < numChannels; ++iChannel)
        {
            for (auto iBand = 0; iBand < Bands::kNumBands; ++iBand)
            {
                for (auto iBin = 0; iBin < numBins; ++iBin)
                {
                    auto numBinSamples = std::min(numSamplesPerBin, numSamples - iBin * numSamplesPerBin);
                    auto normalization = 1.0f;

                    if (type == ReconstructionType::Linear)
                    {
                        auto energy = 0.0f;
                        if (fabsf(energyField[iChannel][iBand][iBin]) >= kEnergyThreshold && fabsf(energyField[0][iBand][iBin]) >= kEnergyThreshold)
                        {
                            energy = energyField[iChannel][iBand][iBin] / sqrtf(energyField[0][iBand][iBin] * sqrtf(4.0f * Math::kPi));
                        }

                        auto prevEnergy = 0.0f;
                        if (iBin == 0)
                        {
                            prevEnergy = energy;
                        }
                        else if (fabsf(energyField[iChannel][iBand][iBin - 1]) >= kEnergyThreshold && fabsf(energyField[0][iBand][iBin - 1]) >= kEnergyThreshold)
                        {
                            prevEnergy = energyField[iChannel][iBand][iBin - 1] / sqrtf(energyField[0][iBand][iBin - 1] * sqrtf(4.0f * Math::kPi));
                        }

                        for (auto iBinSample = 0, iSample = iBin * numSamplesPerBin; iBinSample < numBinSamples; ++iBinSample, ++iSample)
                        {
                            auto weight = static_cast<float>(iBinSample) / static_cast<float>(numSamplesPerBin);
                            auto sampleEnergy = (1.0f - weight) * prevEnergy + weight * energy;

                            mBandIRs[iBand][iSample] = sampleEnergy * mWhiteNoise[iBand][iSample];
                        }
                    }
                    else if (type == ReconstructionType::Gaussian)
                    {
                        if (fabsf(energyField[iChannel][iBand][iBin]) < kEnergyThreshold || fabsf(energyField[0][iBand][iBin]) < kEnergyThreshold)
                            continue;

                        auto tMean = ((iBin + 0.5f) * numSamplesPerBin) / mSamplingRate;
                        auto tVariance = kMinVariance;

                        auto iSample = iBin * numSamplesPerBin;

                        auto t = iSample / static_cast<float>(mSamplingRate);
                        auto dt = 1.0f / mSamplingRate;
                        auto g = expf(-((t - tMean) * (t - tMean)) / (2.0f * tVariance));
                        auto dg = expf(-(dt * ((2.0f * (t - tMean)) + dt)) / (2.0f * tVariance));
                        auto ddg = expf(-(dt * dt) / tVariance);

                        for (auto iBinSample = 0; iBinSample < numBinSamples; ++iBinSample, ++iSample)
                        {
                            mBandIRs[iBand][iSample] = g * mWhiteNoise[iBand][iSample];

                            g *= dg;
                            dg *= ddg;
                        }

                        normalization = energyField[iChannel][iBand][iBin] / sqrtf(energyField[0][iBand][iBin] * sqrtf(4.0f * Math::kPi));
                        assert(Math::isFinite(normalization));
                    }

                    // 0.5 is for sqrt
                    normalization *= airAbsorptionModels[i].evaluate(0.5f * PropagationMedium::kSpeedOfSound * ((iBin + 0.5f) * numSamplesPerBin * (1.0f / mSamplingRate)), iBand);

                    auto iSample = iBin * numSamplesPerBin;
                    ArrayMath::scale(numBinSamples, &mBandIRs[iBand][iSample], normalization, &mBandIRs[iBand][iSample]);
                }
            }

            for (auto iBand = 0; iBand < Bands::kNumBands; ++iBand)
            {
                mFilters[iBand].reset();
            }

            mFilters[0].apply(numSamples, mBandIRs[0], impulseResponse[iChannel]);
            for (auto iBand = 1; iBand < Bands::kNumBands; ++iBand)
            {
                mFilters[iBand].apply(numSamples, mBandIRs[iBand], mBandIRs[iBand]);
                ArrayMath::add(numSamples, impulseResponse[iChannel], mBandIRs[iBand], impulseResponse[iChannel]);
            }

            if (distanceAttenuationCorrectionCurves[i])
            {
                ArrayMath::multiply(numSamples, impulseResponse[iChannel], distanceAttenuationCorrectionCurves[i], impulseResponse[iChannel]);
            }
        }
    }
}

}
