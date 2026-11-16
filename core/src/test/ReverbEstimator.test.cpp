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
#include <propagation_medium.h>
#include <reverb_estimator.h>

TEST_CASE("Reverb time estimate includes air absorption as an energy decay", "[ReverbEstimator]")
{
    const auto reverbTime = 2.0f;

    // 60 dB of decay in reverbTime seconds, in every band, before air absorption
    ipl::EnergyField energyField(4.0f, 0);
    for (auto bin = 0; bin < energyField.numBins(); ++bin)
    {
        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            energyField[0][band][bin] = powf(10.0f, -6.0f * bin * ipl::EnergyField::kBinDuration / reverbTime);
        }
    }

    ipl::AirAbsorptionModel airAbsorption;
    ipl::Reverb reverb;
    ipl::ReverbEstimator::estimate(energyField, airAbsorption, reverb);

    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        // the model gives the amplitude gain exp(-k d) (as on the direct path), so the energy loses
        // 20 log10(e) k c dB per second
        auto airDecay = 20.0f * log10f(expf(1.0f)) * airAbsorption.coefficients[band] * ipl::PropagationMedium::kSpeedOfSound;
        auto expected = 60.0f / (60.0f / reverbTime + airDecay);

        INFO("band " << band << ": " << reverb.reverbTimes[band] << " s, expected " << expected << " s");
        CHECK(reverb.reverbTimes[band] == Approx(expected).epsilon(0.02));
    }
}
