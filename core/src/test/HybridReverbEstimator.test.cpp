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

#include <vector>

#include <air_absorption.h>
#include <energy_field.h>
#include <hybrid_reverb_estimator.h>
#include <iir.h>
#include <impulse_response.h>
#include <propagation_medium.h>
#include <reconstructor.h>

namespace {

float bandEnergy(const ipl::ImpulseResponse& ir, int band, float startTime, float endTime, int samplingRate)
{
    ipl::IIR filters[ipl::Bands::kNumBands];
    ipl::IIR::bandFilters(filters, samplingRate);
    ipl::IIRFilterer filterer;
    filterer.setFilter(filters[band]);

    std::vector<float> bandIR(ir.numSamples());
    filterer.apply(ir.numSamples(), ir[0], bandIR.data());

    auto energy = 0.0f;
    for (auto i = static_cast<int>(startTime * samplingRate); i < static_cast<int>(endTime * samplingRate); ++i)
    {
        energy += bandIR[i] * bandIR[i];
    }
    return energy;
}

}

TEST_CASE("Hybrid reverb EQ matches the air absorption of the reconstructed IR", "[HybridReverbEstimator]")
{
    const auto samplingRate = 48000;
    const auto duration = 1.0f;
    const auto transitionTime = 0.5f;
    const auto overlapFraction = 0.25f;
    const auto cutoffTime = (1.0f - overlapFraction) * transitionTime;

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

    ipl::Reconstructor reconstructor(duration, 0, samplingRate);
    ipl::HybridReverbEstimator estimator(duration, samplingRate, 1024);

    // high / low band amplitude ratio of the IR just before the cutoff, and of the hybrid EQ
    auto tilts = [&](const ipl::AirAbsorptionModel& airAbsorption, float& irTilt, float& eqTilt)
    {
        ipl::ImpulseResponse ir(duration, 0, samplingRate);
        const ipl::EnergyField* energyFields[] = {&energyField};
        const float* curves[] = {nullptr};
        ipl::ImpulseResponse* irs[] = {&ir};
        reconstructor.reconstruct(1, energyFields, curves, &airAbsorption, irs, ipl::ReconstructionType::Gaussian, duration, 0);

        irTilt = sqrtf(bandEnergy(ir, 2, cutoffTime - 0.02f, cutoffTime, samplingRate) /
                       bandEnergy(ir, 0, cutoffTime - 0.02f, cutoffTime, samplingRate));

        float eq[ipl::Bands::kNumBands];
        auto delay = 0;
        estimator.estimate(&energyField, reverb, airAbsorption, ir, transitionTime, overlapFraction, 0, eq, delay);
        eqTilt = eq[2] / eq[0];
    };

    const float noCoefficients[ipl::Bands::kNumBands] = {};
    float irTiltAir, eqTiltAir, irTiltNoAir, eqTiltNoAir;
    tilts(ipl::AirAbsorptionModel{}, irTiltAir, eqTiltAir);
    tilts(ipl::AirAbsorptionModel(noCoefficients, nullptr, nullptr), irTiltNoAir, eqTiltNoAir);

    // relative to the same field without air (removes the band filters' own tilt)
    auto irDb = 20.0f * log10f(irTiltAir / irTiltNoAir);
    auto eqDb = 20.0f * log10f(eqTiltAir / eqTiltNoAir);

    // the air absorption the reconstructor applies at the cutoff bin
    auto cutoffBin = static_cast<int>(ceilf(cutoffTime / ipl::EnergyField::kBinDuration));
    auto distance = ipl::PropagationMedium::kSpeedOfSound * (cutoffBin + 0.5f) * ipl::EnergyField::kBinDuration;
    ipl::AirAbsorptionModel airAbsorption;
    auto airDb = 10.0f * log10f(airAbsorption.evaluate(distance, 2) / airAbsorption.evaluate(distance, 0));
    INFO("air: " << airDb << " dB, IR: " << irDb << " dB, hybrid EQ: " << eqDb << " dB");

    REQUIRE(irDb < -6.0f); // the reconstructed IR loses its highs (band filter leakage makes it read less than airDb)
    REQUIRE(fabsf(eqDb - airDb) < 0.5f);
}
