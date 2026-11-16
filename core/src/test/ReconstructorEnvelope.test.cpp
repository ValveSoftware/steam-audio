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

#include <cmath>

#include <air_absorption.h>
#include <energy_field.h>
#include <impulse_response.h>
#include <reconstructor.h>

// The energy field holds, in bin k, the energy that arrives between k * 10 ms and (k + 1) * 10 ms, so its amplitude is
// centered in the middle of the bin (the Gaussian reconstruction and the air absorption place it there). The linear
// reconstruction interpolated the amplitude between the *end* of the previous bin and the end of the bin, which put the
// energy of an isolated bin 5 ms late: its center of gravity was at the end of the bin, and only half of its energy
// stayed in the bin.
TEST_CASE("The linear reconstruction puts the energy of a bin in the bin", "[Reconstructor]")
{
    const auto samplingRate = 48000;
    const auto duration = 0.5f;
    const auto order = 0;
    const auto bin = 20;

    ipl::EnergyField energyField(duration, order);
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        energyField[0][band][bin] = 0.01f;
    }

    float noAbsorption[ipl::Bands::kNumBands] = {};
    ipl::AirAbsorptionModel airAbsorption(noAbsorption, nullptr, nullptr);

    ipl::ImpulseResponse impulseResponse(duration, order, samplingRate);
    ipl::Reconstructor reconstructor(duration, order, samplingRate);

    const ipl::EnergyField* energyFields[] = { &energyField };
    const float* distanceAttenuationCorrectionCurves[] = { nullptr };
    ipl::ImpulseResponse* impulseResponses[] = { &impulseResponse };
    reconstructor.reconstruct(1, energyFields, distanceAttenuationCorrectionCurves, &airAbsorption, impulseResponses,
                              ipl::ReconstructionType::Linear, duration, order);

    const auto numSamplesPerBin = static_cast<int>(std::ceil(ipl::EnergyField::kBinDuration * samplingRate));

    auto total = 0.0;
    auto inBin = 0.0;
    auto moment = 0.0;
    for (auto i = 0; i < impulseResponse.numSamples(); ++i)
    {
        const auto energy = static_cast<double>(impulseResponse[0][i]) * impulseResponse[0][i];
        total += energy;
        moment += energy * (i + 0.5) / samplingRate;
        if (i / numSamplesPerBin == bin)
        {
            inBin += energy;
        }
    }

    const auto center = (bin + 0.5) * ipl::EnergyField::kBinDuration;

    INFO("the bin holds " << 100.0 * inBin / total << " % of the energy, its center of gravity is at "
         << 1000.0 * moment / total << " ms (center of the bin: " << 1000.0 * center << " ms)");
    CHECK(inBin / total > 0.8);
    CHECK(std::fabs(moment / total - center) < 0.001);
}
