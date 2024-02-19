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

#include <random>

#include <probe_manager.h>
using namespace ipl;

TEST_CASE("Weight function sums to 1", "[Probe]")
{
    ProbeBatch probeBatch;
    probeBatch.addProbe(Sphere(Vector3f(5.0f, 0.0f, 0.0f), std::numeric_limits<float>::max()));
    probeBatch.addProbe(Sphere(Vector3f(0.1f, 0.0f, 0.0f), std::numeric_limits<float>::max()));
    probeBatch.addProbe(Sphere(Vector3f(200.0f, 0.0f, 0.0f), std::numeric_limits<float>::max()));
    probeBatch.addProbe(Sphere(Vector3f(0.9f, 0.0f, 0.0f), std::numeric_limits<float>::max()));
    probeBatch.addProbe(Sphere(Vector3f(20.0f, 0.0f, 0.0f), std::numeric_limits<float>::max()));

    ProbeNeighborhood probeNeighborhood;
    probeNeighborhood.resize(probeBatch.numProbes());
    for (auto i = 0; i < probeBatch.numProbes(); ++i)
    {
        probeNeighborhood.batches[i] = &probeBatch;
        probeNeighborhood.probeIndices[i] = i;
    }

    probeNeighborhood.calcWeights(Vector3f::kZero);

    float weightSum = 0.0f;
    for (auto i = 0; i < probeBatch.numProbes(); ++i)
    {
        weightSum += probeNeighborhood.weights[i];
    }

    REQUIRE(weightSum == Approx(1.0f));
}

TEST_CASE("ProbeTree::getSamplesThatInfluence returns correct number of probes", "[ProbeTree]")
{
    vector<Probe> probes;
    probes.push_back(Probe{ Sphere(Vector3f(-8, -8, -8), 4) });
    probes.push_back(Probe{ Sphere(Vector3f(-6, -6, -6), 4) });
    probes.push_back(Probe{ Sphere(Vector3f(4, 4, 4), 4) });
    probes.push_back(Probe{ Sphere(Vector3f(32, 32, 32), 8) });

    ProbeTree tree(static_cast<int>(probes.size()), probes.data());

    Array<int> probeIndices(probes.size());
    for (auto i = 0u; i < probes.size(); ++i)
    {
        probeIndices[i] = -1;
    }

    tree.getInfluencingProbes(Vector3f(-7, -7, -7), probes.data(), static_cast<int>(probes.size()), probeIndices.data());

    auto numValidProbes = 0;
    for (auto i = 0u; i < probes.size(); ++i)
    {
        if (probeIndices[i] >= 0)
        {
            ++numValidProbes;
        }
    }

    REQUIRE(numValidProbes == 2);
}
