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

#include <custom_scene.h>
#include <path_simulator.h>
using namespace ipl;

static void IPL_CALLBACK alwaysOccluded(const Ray* ray, float minDistance, float maxDistance, uint8_t* occluded, void* userData)
{
    *occluded = 1;
}

TEST_CASE("PathSimulator::findPaths overwrites stale outputs when no probes are valid", "[PathSimulator]")
{
    // One probe, nowhere near the source or the listener, and empty neighborhoods: no valid probes on either end.
    ProbeBatch probes;
    probes.addProbe(Sphere(Vector3f(100.0f, 0.0f, 0.0f), 1.0f));
    probes.commit();

    ProbeNeighborhood sourceProbes;
    ProbeNeighborhood listenerProbes;

    // Source and listener are occluded from each other, so findPaths can't use the direct path either.
    CustomScene scene(nullptr, alwaysOccluded, nullptr, nullptr, nullptr);

    PathSimulator simulator(probes, 1, false, Vector3f(0.0f, -1.0f, 0.0f));

    // Outputs left over from an earlier update in which a path existed.
    const auto order = 1;
    float coeffs[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    float eqGains[Bands::kNumBands] = { 0.3f, 0.3f, 0.3f };
    auto direction = Vector3f(1.0f, 0.0f, 0.0f);
    auto distanceRatio = 0.7f;
    auto totalDeviation = 2.0f;

    auto result = simulator.findPaths(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(10.0f, 0.0f, 0.0f), scene, probes,
                                      sourceProbes, listenerProbes, 1.0f, 0.1f, 1000.0f, order, false, false, false,
                                      false, eqGains, coeffs, DistanceAttenuationModel(), DeviationModel(), &direction,
                                      &distanceRatio, &totalDeviation);

    REQUIRE(!result);

    // No paths: no sound field, and the EQ and deviation of an empty path set, not the stale values.
    for (auto i = 0; i < 4; ++i)
    {
        REQUIRE(coeffs[i] == 0.0f);
    }
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        REQUIRE(eqGains[i] == 1.0f);
    }
    REQUIRE(totalDeviation == 0.0f);
    REQUIRE(distanceRatio == 0.0f);
}
