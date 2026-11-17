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
#include <vector>

#include <air_absorption.h>
#include <energy_field.h>
#include <impulse_response.h>
#include <reconstructor.h>
#include <sh.h>

// The energy field of a source is in units of a source of power 1: reflections of a source 100 m away have an energy of
// about 1e-6 per bin, and a quiet part of the response much less. The reconstructor left every bin under 1e-7 out, so
// the impulse response of a distant source (or of a tail that is under -60 dB of a source at 1 m) was empty, although the
// sound that it is applied to may be as loud as the direct sound of a nearby one (a gunshot has a power that is hundreds
// of times that of a source of level 1). The impulse response has to follow the energy field at any level.
TEST_CASE("The impulse response follows the energy field whatever its level", "[Reconstructor]")
{
    const auto samplingRate = 48000;
    const auto duration = 1.0f;
    const auto order = 0;
    const auto numBins = 50;
    const auto omni = ipl::SphericalHarmonics::evaluate(0, 0, ipl::Vector3f{});

    auto levelOf = [&](float energy)
    {
        ipl::EnergyField energyField(duration, order);
        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            for (auto bin = 0; bin < numBins; ++bin)
            {
                energyField[0][band][bin] = omni * energy;
            }
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

        auto sum = 0.0;
        for (auto i = 0; i < impulseResponse.numSamples(); ++i)
        {
            sum += static_cast<double>(impulseResponse[0][i]) * impulseResponse[0][i];
        }

        return 10.0 * std::log10(std::max(sum, 1e-300));
    };

    const auto reference = levelOf(1e-3f);

    for (auto exponent : { 6, 9, 12 })
    {
        const auto energy = std::pow(10.0f, -static_cast<float>(exponent));
        const auto level = levelOf(energy);
        const auto expected = reference - 10.0 * (exponent - 3);

        INFO("energy " << energy << " per bin: impulse response " << level << " dB, expected " << expected << " dB");
        CHECK(std::fabs(level - expected) < 1.0);
    }
}
