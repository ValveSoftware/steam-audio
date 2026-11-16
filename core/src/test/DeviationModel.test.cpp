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

#include <deviation.h>

using namespace ipl;

// The deviation angle of a path is the sum of the turning angles at its probes, so it can be larger than pi (and than
// 2 pi). A path that bends more must never be louder than one that bends less. The UTD formula is only defined up to
// pi, and it is periodic beyond that: a path turning by 330 degrees in total used to be attenuated like one turning by
// 30 degrees, and one turning by exactly 360 degrees was boosted by more than 12 dB.
TEST_CASE("Deviation gain never increases as the path bends more", "[DeviationModel]")
{
    const auto pi = 3.14159265358979f;
    DeviationModel model;

    for (auto band = 0; band < Bands::kNumBands; ++band)
    {
        auto reference = model.evaluate(1e-8f, band);
        auto previous = model.evaluate(0.5f * pi / 180.0f, band);
        auto worstRise = 0.0f;
        auto worstAngle = 0.0f;

        // Every half degree up to two full turns, plus the exact multiples of a right angle that a probe grid makes.
        for (auto degrees = 1.0f; degrees <= 720.0f; degrees += 0.5f)
        {
            auto value = model.evaluate(degrees * pi / 180.0f, band);
            if (value / previous > 1.0f + worstRise)
            {
                worstRise = value / previous - 1.0f;
                worstAngle = degrees;
            }
            previous = value;
        }

        for (auto quarterTurns = 1; quarterTurns <= 8; ++quarterTurns)
        {
            auto angle = 0.0f;
            for (auto i = 0; i < quarterTurns; ++i)
                angle += 0.5f * pi;

            auto value = model.evaluate(angle, band) / reference;
            INFO("band " << band << ", " << quarterTurns << " right angles (" << angle * 180.0f / pi << " degrees): " << 20.0f * log10f(value) << " dB");
            REQUIRE(value <= 1.0f);
        }

        INFO("band " << band << ": largest rise between two angles half a degree apart: " << 100.0f * worstRise << " % at " << worstAngle << " degrees");
        REQUIRE(worstRise < 1e-3f);
    }
}
