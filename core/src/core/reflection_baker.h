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

#include "energy_field.h"
#include "opencl_device.h"
#include "probe_batch.h"
#include "probe_data.h"
#include "reflection_simulator.h"
#include "reverb_estimator.h"
#include "scene_factory.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// ReflectionBaker
// ---------------------------------------------------------------------------------------------------------------------

class ReflectionBaker
{
public:
    static void bake(const IScene& scene,
                     IReflectionSimulator& simulator,
                     const BakedDataIdentifier& identifier,
                     bool bakeConvolution,
                     bool bakeParametric,
                     int numRays,
                     int numBounces,
                     float simDuration,
                     float bakeDuration,
                     int order,
                     float irradianceMinDistance,
                     int numThreads,
                     int bakeBatchSize,
                     SceneType sceneType,
                     shared_ptr<OpenCLDevice> openCL,
                     ProbeBatch& probeBatch,
                     ProgressCallback callback = nullptr,
                     void* userData = nullptr);

    static void cancel();

private:
    static std::atomic<bool> sCancel;
    static std::atomic<bool> sBakeInProgress;
};

}
