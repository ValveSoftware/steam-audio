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

#include <air_absorption.h>
#include <energy_field.h>
#include <hybrid_reverb_estimator.h>
#include <impulse_response.h>
#include <propagation_medium.h>

TEST_CASE("Hybrid reverb EQ applies air absorption at the cutoff bin", "[HybridReverbEstimator]")
{
    const auto samplingRate = 48000;
    const auto duration = 1.0f;
    const auto transitionTime = 0.5f;
    const auto overlapFraction = 0.25f;

    // same energy in every band
    ipl::EnergyField energyField(duration, 0);
    for (auto bin = 0; bin < energyField.numBins(); ++bin)
    {
        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            energyField[0][band][bin] = expf(-bin * ipl::EnergyField::kBinDuration / 0.2f);
        }
    }

    ipl::Reverb reverb;
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        reverb.reverbTimes[band] = 1.0f;
    }

    ipl::HybridReverbEstimator estimator(duration, samplingRate, 1024);

    auto estimateEQ = [&](const ipl::AirAbsorptionModel& airAbsorption, float* eq)
    {
        ipl::ImpulseResponse ir(duration, 0, samplingRate);
        auto delay = 0;
        estimator.estimate(&energyField, reverb, airAbsorption, ir, transitionTime, overlapFraction, 0, eq, delay);
    };

    const float noCoefficients[ipl::Bands::kNumBands] = {};
    ipl::AirAbsorptionModel airAbsorption;
    float eqAir[ipl::Bands::kNumBands];
    float eqNoAir[ipl::Bands::kNumBands];
    estimateEQ(airAbsorption, eqAir);
    estimateEQ(ipl::AirAbsorptionModel(noCoefficients, nullptr, nullptr), eqNoAir);

    // the EQ is an amplitude, and so is the air absorption model (it is applied as such on the direct path)
    auto cutoffBin = static_cast<int>(ceilf((1.0f - overlapFraction) * transitionTime / ipl::EnergyField::kBinDuration));
    auto distance = ipl::PropagationMedium::kSpeedOfSound * (cutoffBin + 0.5f) * ipl::EnergyField::kBinDuration;
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        auto eqDb = 20.0f * log10f(eqAir[band] / eqNoAir[band]);
        auto airDb = 20.0f * log10f(airAbsorption.evaluate(distance, band));
        INFO("band " << band << ": air " << airDb << " dB, hybrid EQ " << eqDb << " dB");
        CHECK(fabsf(eqDb - airDb) < 0.1f);
    }
}
