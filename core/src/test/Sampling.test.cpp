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

#include <vector>

#include <catch.hpp>

#include <sampling.h>

TEST_CASE("Any prefix of the sphere samples covers all azimuths", "[Samples]")
{
    // The reflection simulators generate maxNumRays samples and trace the first numRays.
    std::vector<ipl::Vector3f> samples(8192);
    ipl::Sampling::generateSphereSamples(8192, samples.data());

    for (auto numRays : {16, 256, 2048})
    {
        int quadrants[4] = {};
        for (auto i = 0; i < numRays; ++i)
            quadrants[(samples[i].x() >= 0.0f ? 0 : 1) + (samples[i].z() >= 0.0f ? 0 : 2)]++;

        for (auto count : quadrants)
            REQUIRE(count > numRays / 8);
    }
}
