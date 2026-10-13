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

#include <propagation_medium.h>
#include <reverb_estimator.h>

// The reflection simulator's energy fields do not include air absorption: Reconstructor applies it to the impulse
// response (amplitude scaled by evaluate(0.5 * c * t), energy by its square). The reverb times estimated from an
// energy field must describe that impulse response, so they must include the decay that air absorption adds.
TEST_CASE("ReverbEstimator accounts for air absorption", "[ReverbEstimator]")
{
    const auto roomReverbTime = 4.0f; // decay of the energy field itself, in every band

    ipl::EnergyField energyField(8.0f, 0);
    energyField.reset();
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        for (auto bin = 0; bin < energyField.numBins(); ++bin)
        {
            auto t = (bin + 0.5f) * ipl::EnergyField::kBinDuration;
            energyField[0][band][bin] = powf(10.0f, -6.0f * t / roomReverbTime);
        }
    }

    SECTION("No air absorption")
    {
        const float zero[ipl::Bands::kNumBands] = {};
        ipl::AirAbsorptionModel airAbsorption(zero, nullptr, nullptr);
        ipl::Reverb reverb;
        ipl::ReverbEstimator::estimate(energyField, airAbsorption, reverb);

        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            REQUIRE(reverb.reverbTimes[band] == Approx(roomReverbTime).epsilon(0.02));
        }
    }

    SECTION("Default air absorption")
    {
        ipl::AirAbsorptionModel airAbsorption;
        ipl::Reverb reverb;
        ipl::ReverbEstimator::estimate(energyField, airAbsorption, reverb);

        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            // Energy decays by an additional 10 log10(e) * coefficient * c dB per second.
            auto airDecayRate = 10.0f * log10f(expf(1.0f)) * airAbsorption.coefficients[band] * ipl::PropagationMedium::kSpeedOfSound;
            auto expectedReverbTime = 60.0f / ((60.0f / roomReverbTime) + airDecayRate);

            REQUIRE(reverb.reverbTimes[band] == Approx(expectedReverbTime).epsilon(0.02));
        }
    }
}
