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

#include "baked_reflection_simulator.h"
#include "profiler.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// BakedReflectionSimulator
// ---------------------------------------------------------------------------------------------------------------------

void BakedReflectionSimulator::findUniqueProbeBatches(const ProbeNeighborhood& neighborhood, unordered_set<const ProbeBatch*>& batches)
{
    batches.clear();

    for (auto i = 0; i < neighborhood.numProbes(); ++i)
    {
        if (!neighborhood.batches[i])
            continue;

        if (batches.find(neighborhood.batches[i]) != batches.end())
            continue;

        batches.insert(neighborhood.batches[i]);
    }
}

void BakedReflectionSimulator::lookupEnergyField(const BakedDataIdentifier& identifier,
                                                 const ProbeNeighborhood& probeNeighborhood,
                                                 const unordered_set<const ProbeBatch*>& uniqueBatches,
                                                 EnergyField& energyField)
{
    PROFILE_FUNCTION();

    assert(identifier.type == BakedDataType::Reflections);

    if (probeNeighborhood.hasValidProbes())
    {
        energyField.reset();

        for (const auto* batch : uniqueBatches)
        {
            if (!batch->hasData(identifier))
                continue;

            auto& data = (IBakedReflectionsLookup&) ((*batch)[identifier]);
            data.evaluateEnergyField(probeNeighborhood, energyField);
        }
    }
}

void BakedReflectionSimulator::lookupReverb(const BakedDataIdentifier& identifier,
                                            const ProbeNeighborhood& probeNeighborhood,
                                            const unordered_set<const ProbeBatch*>& uniqueBatches,
                                            Reverb& reverb)
{
    PROFILE_FUNCTION();
        
    assert(identifier.type == BakedDataType::Reflections);

    if (probeNeighborhood.hasValidProbes())
    {
        memset(&reverb, 0, sizeof(Reverb));

        for (const auto* batch : uniqueBatches)
        {
            if (!batch->hasData(identifier))
                continue;

            auto& data = (IBakedReflectionsLookup&) ((*batch)[identifier]);
            data.evaluateReverb(probeNeighborhood, reverb);
        }
    }
}

}
