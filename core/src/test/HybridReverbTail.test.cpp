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

// Power per sample in a band of a response, between two times, in dB. Only the middle of the band is measured: the band
// filters of the reconstructor and the filters of the reverb do not have the same slopes, so near the edges of a band
// the two responses get different amounts of the neighbouring bands, which decay at other rates.
double bandLevel(const std::vector<float>& response, int band, double startTime, double endTime)
{
    ipl::IIR filter;
    if (band == 0)
        filter = ipl::IIR::lowPass(250.0f, kSamplingRate);
    else if (band == 1)
        filter = ipl::IIR::bandPass(2000.0f, 4000.0f, kSamplingRate);
    else
        filter = ipl::IIR::highPass(14000.0f, kSamplingRate);

    std::vector<float> bandResponse(response);
    for (auto pass = 0; pass < 2; ++pass)
    {
        ipl::IIRFilterer filterer;
        filterer.setFilter(filter);
        filterer.apply(static_cast<int>(bandResponse.size()), bandResponse.data(), bandResponse.data());
    }

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
// the hybrid reverb to a unit impulse, and, for comparison, the same impulse response without the transition, and the
// part of it that each band of the energy field makes on its own.
void renderHybridAndReference(const float* reverbTimes,
                              float transitionTime,
                              float overlapFraction,
                              std::vector<float>& hybrid,
                              std::vector<float>& reference,
                              std::vector<float>* bandReferences)
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

    // The band filters of the reconstructor are single biquads (6 dB per octave out of the band), so in the impulse
    // response a band that decays faster than its neighbour soon reads the level of the neighbour. Each band alone gives
    // the level that the tail has to follow. The reconstructor uses the same noise for a band whatever the other bands
    // hold, so these add up to the impulse response.
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        ipl::EnergyField bandEnergyField(duration, order);
        bandEnergyField.reset();
        for (auto bin = 0; bin < energyField.numBins(); ++bin)
        {
            bandEnergyField[0][band][bin] = energyField[0][band][bin];
        }

        ipl::ImpulseResponse bandImpulseResponse(duration, order, kSamplingRate);
        const ipl::EnergyField* bandEnergyFields[] = { &bandEnergyField };
        ipl::ImpulseResponse* bandImpulseResponses[] = { &bandImpulseResponse };
        reconstructor.reconstruct(1, bandEnergyFields, distanceAttenuationCorrectionCurves, &airAbsorption, bandImpulseResponses,
                                  ipl::ReconstructionType::Linear, duration, order);

        bandReferences[band].assign(bandImpulseResponse[0], bandImpulseResponse[0] + bandImpulseResponse.numSamples());
    }

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
    const auto transitionTime = 0.5f;
    const auto overlapFraction = 0.25f;

    // The same reverb time in every band, and different ones (the tone correction of the reverb depends on them).
    const float flatReverbTimes[ipl::Bands::kNumBands] = { 1.5f, 1.5f, 1.5f };
    const float tiltedReverbTimes[ipl::Bands::kNumBands] = { 2.0f, 1.5f, 1.0f };
    const float steepReverbTimes[ipl::Bands::kNumBands] = { 3.0f, 2.0f, 1.0f };

    for (const auto* reverbTimes : { flatReverbTimes, tiltedReverbTimes, steepReverbTimes })
    {
        std::vector<float> hybrid, reference, bandReferences[ipl::Bands::kNumBands];
        renderHybridAndReference(reverbTimes, transitionTime, overlapFraction, hybrid, reference, bandReferences);

        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            INFO("reverb times " << reverbTimes[0] << " " << reverbTimes[1] << " " << reverbTimes[2] << " s, band " << band);

            // Up to the start of the transition, the hybrid reverb is the impulse response.
            CHECK(std::fabs(bandLevel(hybrid, band, 0.2, 0.35) - bandLevel(reference, band, 0.2, 0.35)) < 0.1);

            // After it, the tail has to follow the impulse response. The tail is a feedback delay network, so it only
            // approximates the level of the impulse response (it is made of three bands only), but it was 5 to 16 dB
            // under it when its gain did not take the reverb into account. It is compared to the part of the impulse
            // response that the band makes: in the whole response, a band that decays faster than its neighbour reads
            // the neighbour's level through the overlap of the reconstructor's band filters (3 dB over the tail for the
            // highest band at 2 / 1.5 / 1 s, 6 dB at 3 / 2 / 1 s), which the tail does not have.
            const auto difference = bandLevel(hybrid, band, 0.55, 0.75) - bandLevel(bandReferences[band], band, 0.55, 0.75);
            INFO("tail minus impulse response: " << difference << " dB");
            CHECK(std::fabs(difference) < 3.0);
        }
    }
}
