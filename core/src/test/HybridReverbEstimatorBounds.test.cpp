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

#include <energy_field.h>
#include <hybrid_reverb_estimator.h>
#include <impulse_response.h>
#include <reverb_effect.h>

using namespace ipl;

// With an overlap fraction that is small, the cutoff bin of the estimator is the first bin that is past the end of the
// energy field, which is bin 0 of the next band: the equalization of band 0 was read from there.
TEST_CASE("The hybrid reverb estimator reads the cutoff bin inside the energy field", "[HybridReverbEstimator]")
{
    const auto samplingRate = 48000;
    const auto duration = 0.5f;
    const auto order = 1;
    const auto overlapFraction = 0.005f;

    EnergyField energyField(duration, order);

    // Bin 0 of band 1 holds a marker, and the rest of the field is empty: reading one bin past the end of band 0 finds it.
    energyField[0][1][0] = 1.0f;

    ImpulseResponse impulseResponse(duration, order, samplingRate);
    HybridReverbEstimator estimator(duration, samplingRate, 512);

    Reverb reverb;
    for (auto band = 0; band < Bands::kNumBands; ++band)
    {
        reverb.reverbTimes[band] = 1.0f;
    }

    float eq[Bands::kNumBands];
    auto delay = 0;
    estimator.estimate(&energyField, reverb, impulseResponse, duration, overlapFraction, order, eq, delay);

    INFO("equalization of the bands: " << eq[0] << " " << eq[1] << " " << eq[2] << " (the field is empty at the cutoff bin)");
    CHECK(eq[0] == 0.0f);
}
