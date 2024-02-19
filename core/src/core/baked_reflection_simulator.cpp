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

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// BakedReflectionSimulator
// ---------------------------------------------------------------------------------------------------------------------

void BakedReflectionSimulator::lookupEnergyField(const BakedDataIdentifier& identifier,
                                                 const ProbeNeighborhood& probeNeighborhood,
                                                 EnergyField& energyField)
{
    assert(identifier.type == BakedDataType::Reflections);

    if (probeNeighborhood.hasValidProbes())
    {
        energyField.reset();

        for (auto i = 0; i < probeNeighborhood.numProbes(); ++i)
        {
            if (probeNeighborhood.batches[i] &&
                probeNeighborhood.probeIndices[i] >= 0 &&
                probeNeighborhood.weights[i] > 0.0f &&
                probeNeighborhood.batches[i]->hasData(identifier))
            {
                auto& data = static_cast<BakedReflectionsData&>((*probeNeighborhood.batches[i])[identifier]);
                const auto* probeEnergyField = data.lookupEnergyField(probeNeighborhood.probeIndices[i]);
                if (probeEnergyField)
                {
                    EnergyField::scaleAccumulate(*probeEnergyField, probeNeighborhood.weights[i], energyField);
                }
            }
        }
    }
}

void BakedReflectionSimulator::lookupReverb(const BakedDataIdentifier& identifier,
                                            const ProbeNeighborhood& probeNeighborhood,
                                            Reverb& reverb)
{
    assert(identifier.type == BakedDataType::Reflections);

    if (probeNeighborhood.hasValidProbes())
    {
        memset(&reverb, 0, sizeof(Reverb));

        for (auto i = 0; i < probeNeighborhood.numProbes(); ++i)
        {
            if (probeNeighborhood.batches[i] &&
                probeNeighborhood.probeIndices[i] >= 0 &&
                probeNeighborhood.weights[i] > 0.0f &&
                probeNeighborhood.batches[i]->hasData(identifier))
            {
                auto& data = static_cast<BakedReflectionsData&>((*probeNeighborhood.batches[i])[identifier]);
                const auto* probeReverb = data.lookupReverb(probeNeighborhood.probeIndices[i]);
                if (probeReverb)
                {
                    for (auto j = 0; j < Bands::kNumBands; ++j)
                    {
                        reverb.reverbTimes[j] += probeNeighborhood.weights[i] * probeReverb->reverbTimes[j];
                    }
                }
            }
        }
    }
}

}
