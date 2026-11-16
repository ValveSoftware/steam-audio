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

#include <baked_reflection_data.h>
#include <baked_reflection_simulator.h>

using namespace ipl;

// A listener can be inside the probes of several batches, and only some of those batches may carry the baked data
// being looked up. The probes of a batch without the data must not be looked up in the data of another batch: their
// indices mean nothing there.
TEST_CASE("Baked lookups ignore probes of batches that have no such data", "[BakedReflections]")
{
    // Batch X has the baked data, with a reverb time of 1 s (and an energy of 1) at probe 0 and 3 s (energy 3) at
    // probe 1. Batch Y has no baked data at all.
    ProbeBatch x;
    x.addProbe(Sphere(Vector3f(0.0f, 0.0f, 0.0f), 5.0f));
    x.addProbe(Sphere(Vector3f(100.0f, 0.0f, 0.0f), 5.0f));
    x.commit();

    ProbeBatch y;
    y.addProbe(Sphere(Vector3f(200.0f, 0.0f, 0.0f), 5.0f));
    y.addProbe(Sphere(Vector3f(0.0f, 0.0f, 0.5f), 5.0f));
    y.commit();

    BakedDataIdentifier id;
    id.type = BakedDataType::Reflections;
    id.variation = BakedDataVariation::Reverb;

    auto data = make_unique<BakedReflectionsData>(id, x.numProbes(), true, true);
    for (auto i = 0; i < 2; ++i)
    {
        Reverb reverb;
        for (auto b = 0; b < Bands::kNumBands; ++b)
            reverb.reverbTimes[b] = (i == 0) ? 1.0f : 3.0f;
        data->set(i, reverb);

        auto field = make_unique<EnergyField>(0.1f, 0);
        field->reset();
        field->data()[0][0][0] = (i == 0) ? 1.0f : 3.0f;
        data->set(i, std::move(field));
    }
    x.addData(id, std::move(data));

    // The listener is at the center of probe 1 of Y, and 0.5 m from probe 0 of X.
    auto listener = Vector3f(0.0f, 0.0f, 0.5f);

    ProbeNeighborhood neighborhood;
    neighborhood.resize(2 * ProbeNeighborhood::kMaxProbesPerBatch);
    x.getInfluencingProbes(listener, neighborhood, 0);
    y.getInfluencingProbes(listener, neighborhood, ProbeNeighborhood::kMaxProbesPerBatch);
    neighborhood.calcWeights(listener);

    unordered_set<const ProbeBatch*> batches;
    BakedReflectionSimulator::findUniqueProbeBatches(neighborhood, batches);
    REQUIRE(batches.size() == 2);

    // Only probe 0 of X can contribute, with a weight of at most 1: nothing can come out above 1.
    Reverb reverb;
    BakedReflectionSimulator::lookupReverb(id, neighborhood, batches, reverb);
    INFO("reverb time: " << reverb.reverbTimes[0] << " (at most 1 expected)");
    REQUIRE(reverb.reverbTimes[0] <= 1.0f);

    EnergyField field(0.1f, 0);
    BakedReflectionSimulator::lookupEnergyField(id, neighborhood, batches, field);
    INFO("energy: " << field.data()[0][0][0] << " (at most 1 expected)");
    REQUIRE(field.data()[0][0][0] <= 1.0f);
}
