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

#include "reflection_simulator.h"
#include "scene_factory.h"
#include "opencl_device.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ReflectionSimulatorFactory
// --------------------------------------------------------------------------------------------------------------------

namespace ReflectionSimulatorFactory
{
    unique_ptr<IReflectionSimulator> create(SceneType sceneType,
                                            int maxNumRays,
                                            int numDiffuseSamples,
                                            float maxDuration,
                                            int maxOrder,
                                            int maxNumSources,
                                            int maxNumListeners,
                                            int numThreads,
                                            int rayBatchSize,
                                            shared_ptr<RadeonRaysDevice> radeonRays);
};

}