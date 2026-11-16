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
#include <impulse_response.h>
#include <propagation_medium.h>
#include <reconstructor.h>

TEST_CASE("Reconstructed impulse response gets the same air absorption as the direct path", "[AirAbsorption]")
{
    const auto samplingRate = 48000;
    const auto duration = 0.5f;
    const auto numSamplesPerBin = static_cast<int>(ceilf(ipl::EnergyField::kBinDuration * samplingRate));

    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        for (auto bin : {10, 30})
        {
            // energy in a single bin of a single band
            ipl::EnergyField energyField(duration, 0);
            energyField[0][band][bin] = 1.0f;

            auto binEnergy = [&](const ipl::AirAbsorptionModel& airAbsorption)
            {
                ipl::Reconstructor reconstructor(duration, 0, samplingRate);
                ipl::ImpulseResponse ir(duration, 0, samplingRate);
                const ipl::EnergyField* energyFields[] = {&energyField};
                const float* curves[] = {nullptr};
                ipl::ImpulseResponse* irs[] = {&ir};
                reconstructor.reconstruct(1, energyFields, curves, &airAbsorption, irs, ipl::ReconstructionType::Gaussian, duration, 0);

                auto energy = 0.0f;
                for (auto i = 0; i < ir.numSamples(); ++i)
                {
                    energy += ir[0][i] * ir[0][i];
                }
                return energy;
            };

            const float noCoefficients[ipl::Bands::kNumBands] = {};
            ipl::AirAbsorptionModel airAbsorption;
            auto irDb = 10.0f * log10f(binEnergy(airAbsorption) / binEnergy(ipl::AirAbsorptionModel(noCoefficients, nullptr, nullptr)));

            // the direct path applies the model's value as an amplitude gain
            auto distance = ipl::PropagationMedium::kSpeedOfSound * (bin + 0.5f) * numSamplesPerBin / samplingRate;
            auto directDb = 20.0f * log10f(airAbsorption.evaluate(distance, band));

            INFO("band " << band << ", " << distance << " m: impulse response " << irDb << " dB, direct path " << directDb << " dB");
            CHECK(fabsf(irDb - directDb) < 0.1f);
        }
    }
}
