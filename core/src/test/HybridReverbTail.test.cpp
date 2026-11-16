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

#include <catch.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include <air_absorption.h>
#include <audio_buffer.h>
#include <energy_field.h>
#include <hybrid_reverb_effect.h>
#include <hybrid_reverb_estimator.h>
#include <iir.h>
#include <impulse_response.h>
#include <reconstructor.h>

namespace {

const auto kSamplingRate = 48000;
const auto kFrameSize = 256;

// Power per sample in a band of a response, between two times, in dB.
double bandLevel(const std::vector<float>& response, int band, double startTime, double endTime)
{
    ipl::IIR filters[ipl::Bands::kNumBands];
    ipl::IIR::bandFilters(filters, kSamplingRate);

    ipl::IIRFilterer filterer;
    filterer.setFilter(filters[band]);

    std::vector<float> bandResponse(response.size());
    filterer.apply(static_cast<int>(response.size()), response.data(), bandResponse.data());

    const auto start = static_cast<size_t>(startTime * kSamplingRate);
    const auto end = static_cast<size_t>(endTime * kSamplingRate);

    auto sum = 0.0;
    for (auto i = start; i < end; ++i)
    {
        sum += static_cast<double>(bandResponse[i]) * bandResponse[i];
    }

    return 10.0 * std::log10(sum / static_cast<double>(end - start));
}

// Reconstructs an impulse response from an energy field that decays with the given reverb times, then makes it hybrid
// as the reflection simulator does: the response is kept up to the transition, and the rest comes from the parametric
// reverb, which gets the reverb times and the delay that estimate() returns. Returns the omni channel of the response of
// the hybrid reverb to a unit impulse, and, for comparison, the same impulse response without the transition.
void renderHybridAndReference(const float* reverbTimes,
                              float transitionTime,
                              float overlapFraction,
                              std::vector<float>& hybrid,
                              std::vector<float>& reference)
{
    const auto duration = 3.0f;
    const auto order = 0;

    ipl::EnergyField energyField(duration, order);
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        for (auto bin = 0; bin < energyField.numBins(); ++bin)
        {
            const auto time = bin * ipl::EnergyField::kBinDuration;
            // Large enough to stay above the energy under which the reconstructor leaves a bin out.
            energyField[0][band][bin] = 1000.0f * powf(10.0f, -6.0f * time / reverbTimes[band]);
        }
    }

    float noAbsorption[ipl::Bands::kNumBands] = {};
    ipl::AirAbsorptionModel airAbsorption(noAbsorption, nullptr, nullptr);

    ipl::ImpulseResponse impulseResponse(duration, order, kSamplingRate);
    ipl::Reconstructor reconstructor(duration, order, kSamplingRate);

    const ipl::EnergyField* energyFields[] = { &energyField };
    const float* distanceAttenuationCorrectionCurves[] = { nullptr };
    ipl::ImpulseResponse* impulseResponses[] = { &impulseResponse };
    reconstructor.reconstruct(1, energyFields, distanceAttenuationCorrectionCurves, &airAbsorption, impulseResponses,
                              ipl::ReconstructionType::Linear, duration, order);

    reference.assign(impulseResponse[0], impulseResponse[0] + impulseResponse.numSamples());

    ipl::Reverb reverb;
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        reverb.reverbTimes[band] = reverbTimes[band];
    }

    ipl::HybridReverbEstimator estimator(duration, kSamplingRate, kFrameSize);

    float eqCoeffs[ipl::Bands::kNumBands];
    auto delay = 0;
    estimator.estimate(&energyField, reverb, impulseResponse, transitionTime, overlapFraction, order, eqCoeffs, delay);

    // The convolved part of the hybrid reverb, for an impulse, is the (modified) impulse response. Add the tail.
    hybrid.assign(impulseResponse[0], impulseResponse[0] + impulseResponse.numSamples());

    ipl::HybridReverbEffect effect(ipl::AudioSettings{ kSamplingRate, kFrameSize }, ipl::HybridReverbEffectSettings{ 1, impulseResponse.numSamples() });

    ipl::AudioBuffer in(1, kFrameSize);
    ipl::AudioBuffer out(1, kFrameSize);

    ipl::HybridReverbEffectDirectParams params{};
    params.reverb = &reverb;
    params.eqCoeffs = eqCoeffs;
    params.delay = delay;
    params.numChannels = 1;
    params.numSamples = impulseResponse.numSamples();

    const auto numFrames = static_cast<int>(hybrid.size()) / kFrameSize;
    for (auto frame = 0; frame < numFrames; ++frame)
    {
        in.makeSilent();
        if (frame == 0)
        {
            in[0][0] = 1.0f;
        }

        effect.apply(params, in, out);

        for (auto i = 0; i < kFrameSize; ++i)
        {
            hybrid[frame * kFrameSize + i] += out[0][i];
        }
    }
}

}

// The parametric tail of a hybrid reverb takes over from the convolved part at the transition. It has to start at the
// level that the impulse response had there, whatever the reverb times, or the reverb has a step at the transition.
TEST_CASE("The parametric tail of the hybrid reverb starts at the level of the impulse response", "[HybridReverb]")
{
    const auto transitionTime = 1.0f;
    const auto overlapFraction = 0.25f;

    // The same reverb time in every band, and different ones (the tone correction of the reverb depends on them).
    const float flatReverbTimes[ipl::Bands::kNumBands] = { 1.5f, 1.5f, 1.5f };
    const float tiltedReverbTimes[ipl::Bands::kNumBands] = { 2.0f, 1.5f, 1.0f };

    for (const auto* reverbTimes : { flatReverbTimes, tiltedReverbTimes })
    {
        std::vector<float> hybrid, reference;
        renderHybridAndReference(reverbTimes, transitionTime, overlapFraction, hybrid, reference);

        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            INFO("reverb times " << reverbTimes[0] << " " << reverbTimes[1] << " " << reverbTimes[2] << " s, band " << band);

            // Up to the start of the transition, the hybrid reverb is the impulse response.
            CHECK(std::fabs(bandLevel(hybrid, band, 0.4, 0.7) - bandLevel(reference, band, 0.4, 0.7)) < 0.1);

            // After it, the tail has to follow the impulse response. The tail is a feedback delay network, so it only
            // approximates the level of the impulse response (it decays a little slower, and it is made of three bands
            // only), but it was 5 to 16 dB under it when its gain did not take the reverb into account.
            const auto difference = bandLevel(hybrid, band, 1.05, 1.25) - bandLevel(reference, band, 1.05, 1.25);
            CHECK(std::fabs(difference) < 6.0);
        }
    }
}
