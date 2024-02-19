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

#include "reflection_baker.h"
#include "reflection_simulator_factory.h"
#include "path_data.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_scene.h"
#include "api_probes.h"
#include "api_radeonrays_device.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

void CContext::bakeReflections(IPLReflectionsBakeParams* params,
                               IPLProgressCallback progressCallback,
                               void* userData)
{
    if (!params || !params->scene || !params->probeBatch)
        return;

    auto _scene = reinterpret_cast<CScene*>(params->scene)->mHandle.get();
    auto _probeBatch = reinterpret_cast<CProbeBatch*>(params->probeBatch)->mHandle.get();
    if (!_scene || !_probeBatch || _probeBatch->numProbes() == 0)
        return;

    auto _sceneType = static_cast<SceneType>(params->sceneType);
    auto _identifier = *reinterpret_cast<BakedDataIdentifier*>(&params->identifier);
    auto _bakeConvolution = (params->bakeFlags & IPL_REFLECTIONSBAKEFLAGS_BAKECONVOLUTION);
    auto _bakeParametric = (params->bakeFlags & IPL_REFLECTIONSBAKEFLAGS_BAKEPARAMETRIC);

    auto _bakeBatchSize = params->bakeBatchSize;
    if (params->sceneType != IPL_SCENETYPE_RADEONRAYS && params->identifier.variation != IPL_BAKEDDATAVARIATION_STATICLISTENER)
    {
        // Batching is only supported if either a) we're baking on the GPU using Radeon Rays, or b) we're baking
        // static listener reflections.
        _bakeBatchSize = 1;
    }

    auto maxNumSources = 1;
    auto maxNumListeners = 1;
    if (params->identifier.variation == IPL_BAKEDDATAVARIATION_STATICSOURCE)
    {
        maxNumListeners = _bakeBatchSize;
    }
    else if (params->identifier.variation == IPL_BAKEDDATAVARIATION_STATICLISTENER)
    {
        maxNumSources = _bakeBatchSize;
    }
    else if (params->identifier.variation == IPL_BAKEDDATAVARIATION_REVERB)
    {
        maxNumSources = _bakeBatchSize;
        maxNumListeners = _bakeBatchSize;
    }

    auto _openCL = (params->openCLDevice) ? reinterpret_cast<COpenCLDevice*>(params->openCLDevice)->mHandle.get() : nullptr;
    auto _radeonRays = (params->radeonRaysDevice) ? reinterpret_cast<CRadeonRaysDevice*>(params->radeonRaysDevice)->mHandle.get() : nullptr;

    auto simulator = ReflectionSimulatorFactory::create(_sceneType, params->numRays, params->numDiffuseSamples,
                                                        params->simulatedDuration, params->order, maxNumSources,
                                                        maxNumListeners, params->numThreads, params->rayBatchSize, _radeonRays);

    ReflectionBaker::bake(*_scene, *simulator, _identifier, _bakeConvolution, _bakeParametric, params->numRays,
                          params->numBounces, params->simulatedDuration, params->savedDuration, params->order,
                          params->irradianceMinDistance, params->numThreads, params->bakeBatchSize, _sceneType, _openCL,
                          *_probeBatch, progressCallback, userData);
}

void CContext::cancelBakeReflections()
{
    ReflectionBaker::cancel();
}

void CContext::bakePaths(IPLPathBakeParams* params,
                         IPLProgressCallback progressCallback,
                         void* userData)
{
    if (!params || !params->scene || !params->probeBatch)
        return;

    auto _scene = reinterpret_cast<CScene*>(params->scene)->mHandle.get();
    auto _probeBatch = reinterpret_cast<CProbeBatch*>(params->probeBatch)->mHandle.get();
    if (!_scene || !_probeBatch || _probeBatch->numProbes() == 0)
        return;

    auto _identifier = *reinterpret_cast<BakedDataIdentifier*>(&params->identifier);
    auto _asymmetricVisRange = true;
    auto _down = Vector3f(0.0f, -1.0f, 0.0f);
    auto _visRangeRealTime = params->visRange;
    auto _pruneVisGraph = false;

    PathBaker::bake(*_scene, _identifier, params->numSamples, params->radius, params->threshold, params->visRange,
                    _visRangeRealTime, params->pathRange, _asymmetricVisRange, _down, _pruneVisGraph,
                    params->numThreads, *_probeBatch, progressCallback, userData);
}

void CContext::cancelBakePaths()
{
    PathBaker::cancel();
}

}
