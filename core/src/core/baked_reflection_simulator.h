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

#pragma once

#include "baked_reflection_data.h"
#include "probe_manager.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// BakedReflectionSimulator
// ---------------------------------------------------------------------------------------------------------------------

namespace BakedReflectionSimulator
{
    void findUniqueProbeBatches(const ProbeNeighborhood& neighborhood, unordered_set<const ProbeBatch*>& batches);

    void lookupEnergyField(const BakedDataIdentifier& identifier,
                           const ProbeNeighborhood& probeNeighborhood,
                           const unordered_set<const ProbeBatch*>& uniqueBatches,
                           EnergyField& energyField);

    void lookupReverb(const BakedDataIdentifier& identifier,
                      const ProbeNeighborhood& probeNeighborhood,
                      const unordered_set<const ProbeBatch*>& uniqueBatches,
                      Reverb& reverb);
};

}
