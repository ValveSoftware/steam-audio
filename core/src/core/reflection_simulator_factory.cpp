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

#include "reflection_simulator_factory.h"

#include "embree_reflection_simulator.h"
#include "radeonrays_reflection_simulator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ReflectionSimulatorFactory
// --------------------------------------------------------------------------------------------------------------------

unique_ptr<IReflectionSimulator> ReflectionSimulatorFactory::create(SceneType sceneType,
                                                                    int maxNumRays,
                                                                    int numDiffuseSamples,
                                                                    float maxDuration,
                                                                    int maxOrder,
                                                                    int maxNumSources,
                                                                    int maxNumListeners,
                                                                    int numThreads,
                                                                    int rayBatchSize,
                                                                    shared_ptr<RadeonRaysDevice> radeonRays)
{
    switch (sceneType)
    {
    case SceneType::Default:
        return ipl::make_unique<ReflectionSimulator>(maxNumRays, numDiffuseSamples, maxDuration, maxOrder, maxNumSources,
                                                     numThreads);

    case SceneType::Custom:
        return ipl::make_unique<BatchedReflectionSimulator>(maxNumRays, numDiffuseSamples, maxDuration, maxOrder,
                                                            maxNumSources, numThreads, rayBatchSize);

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    case SceneType::Embree:
        return ipl::make_unique<EmbreeReflectionSimulator>(maxNumRays, numDiffuseSamples, maxDuration, maxOrder, maxNumSources,
                                                           numThreads);
#endif

#if defined(IPL_USES_RADEONRAYS)
    case SceneType::RadeonRays:
        return ipl::make_unique<RadeonRaysReflectionSimulator>(maxNumRays, numDiffuseSamples, maxDuration, maxOrder,
                                                               maxNumSources, maxNumListeners, radeonRays);
#endif

    default:
        throw Exception(Status::Initialization);
    }
}

}