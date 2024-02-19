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

#include "simulation_data.h"
#include "simulation_manager.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_radeonrays_device.h"
#include "api_tan_device.h"
#include "api_scene.h"
#include "api_probes.h"
#include "api_simulator.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CSimulator
// --------------------------------------------------------------------------------------------------------------------

CSimulator::CSimulator(CContext* context,
                       IPLSimulationSettings* settings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _enableDirect = (settings->flags & IPL_SIMULATIONFLAGS_DIRECT);
    auto _enableIndirect = (settings->flags & IPL_SIMULATIONFLAGS_REFLECTIONS);
    auto _enablePathing = (settings->flags & IPL_SIMULATIONFLAGS_PATHING);
    auto _sceneType = static_cast<SceneType>(settings->sceneType);
    auto _indirectType = static_cast<IndirectEffectType>(settings->reflectionType);
    auto _maxNumListeners = 1;
    auto _asymmetricVisRange = true;
    auto _down = Vector3f(0.0f, -1.0f, 0.0f);
    auto _openCL = (settings->openCLDevice) ? reinterpret_cast<COpenCLDevice*>(settings->openCLDevice)->mHandle.get() : nullptr;
    auto _radeonRays = (settings->radeonRaysDevice) ? reinterpret_cast<CRadeonRaysDevice*>(settings->radeonRaysDevice)->mHandle.get() : nullptr;
    auto _tan = (settings->tanDevice) ? reinterpret_cast<CTrueAudioNextDevice*>(settings->tanDevice)->mHandle.get() : nullptr;

    new (&mHandle) Handle<SimulationManager>(ipl::make_shared<SimulationManager>(_enableDirect, _enableIndirect, _enablePathing,
                                             _sceneType, _indirectType, settings->maxNumOcclusionSamples, settings->maxNumRays,
                                             settings->numDiffuseSamples, settings->maxDuration, settings->maxOrder,
                                             settings->maxNumSources, _maxNumListeners, settings->numThreads,
                                             settings->rayBatchSize, settings->numVisSamples, _asymmetricVisRange, _down,
                                             settings->samplingRate, settings->frameSize, _openCL, _radeonRays, _tan), _context);
}

ISimulator* CSimulator::retain()
{
    mHandle.retain();
    return this;
}

void CSimulator::release()
{
    if (mHandle.release())
    {
        this->~CSimulator();
        gMemory().free(this);
    }
}

void CSimulator::setScene(IScene* scene)
{
    if (!scene)
        return;

    auto _simulator = mHandle.get();
    auto _scene = reinterpret_cast<CScene*>(scene)->mHandle.get();
    if (!_simulator || !_scene)
        return;

    _simulator->scene() = _scene;
}

void CSimulator::addProbeBatch(IProbeBatch* probeBatch)
{
    if (!probeBatch)
        return;

    auto _probeBatch = reinterpret_cast<CProbeBatch*>(probeBatch)->mHandle.get();
    auto _simulator = mHandle.get();
    if (!_probeBatch || !_simulator)
        return;

    _simulator->addProbeBatch(_probeBatch);
}

void CSimulator::removeProbeBatch(IProbeBatch* probeBatch)
{
    if (!probeBatch)
        return;

    auto _probeBatch = reinterpret_cast<CProbeBatch*>(probeBatch)->mHandle.get();
    auto _simulator = mHandle.get();
    if (!_probeBatch || !_simulator)
        return;

    _simulator->removeProbeBatch(_probeBatch);
}

void CSimulator::setSharedInputs(IPLSimulationFlags flags,
                                 IPLSimulationSharedInputs* sharedData)
{
    if (!sharedData)
        return;

    auto _simulator = mHandle.get();
    if (!_simulator)
        return;

    if (flags & IPL_SIMULATIONFLAGS_DIRECT)
    {
        SharedDirectSimulationInputs sharedDirectInputs;
        sharedDirectInputs.listener = *reinterpret_cast<CoordinateSpace3f*>(&sharedData->listener);

        _simulator->setSharedDirectInputs(sharedDirectInputs);
    }
    if (flags & IPL_SIMULATIONFLAGS_REFLECTIONS)
    {
        SharedReflectionSimulationInputs sharedReflectionInputs;
        sharedReflectionInputs.listener = *reinterpret_cast<CoordinateSpace3f*>(&sharedData->listener);
        sharedReflectionInputs.numRays = sharedData->numRays;
        sharedReflectionInputs.numBounces = sharedData->numBounces;
        sharedReflectionInputs.duration = sharedData->duration;
        sharedReflectionInputs.order = sharedData->order;
        sharedReflectionInputs.irradianceMinDistance = sharedData->irradianceMinDistance;
        sharedReflectionInputs.reconstructionType = ReconstructionType::Linear;

        _simulator->setSharedReflectionInputs(sharedReflectionInputs);
    }
    if (flags & IPL_SIMULATIONFLAGS_PATHING)
    {
        SharedPathingSimulationInputs sharedPathingInputs;
        sharedPathingInputs.listener = *reinterpret_cast<CoordinateSpace3f*>(&sharedData->listener);

        if (Context::isCallerAPIVersionAtLeast(4, 3))
        {
            sharedPathingInputs.visCallback = reinterpret_cast<ValidationRayVisualizationCallback>(sharedData->pathingVisCallback);
            sharedPathingInputs.userData = sharedData->pathingUserData;
        }

        _simulator->setSharedPathingInputs(sharedPathingInputs);
    }
}

void CSimulator::commit()
{
    auto _simulator = mHandle.get();
    if (!_simulator)
        return;

    _simulator->commit();
}

void CSimulator::runDirect()
{
    auto _simulator = mHandle.get();
    if (!_simulator)
        return;

    _simulator->simulateDirect();
}

void CSimulator::runReflections()
{
    auto _simulator = mHandle.get();
    if (!_simulator)
        return;

    _simulator->simulateIndirect();
}

void CSimulator::runPathing()
{
    auto _simulator = mHandle.get();
    if (!_simulator)
        return;

    _simulator->simulatePathing();
}

IPLerror CSimulator::createSource(IPLSourceSettings* settings,
                                  ISource** source)
{
    if (!settings || !source)
        return IPL_STATUS_FAILURE;

    auto _context = mHandle.context();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _source = reinterpret_cast<CSource*>(gMemory().allocate(sizeof(CSource), Memory::kDefaultAlignment));
        new (_source) CSource(this, settings);
        *source = _source;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}


// --------------------------------------------------------------------------------------------------------------------
// CSource
// --------------------------------------------------------------------------------------------------------------------

CSource::CSource(CSimulator* simulator,
                 IPLSourceSettings* settings)
{
    auto _context = simulator->mHandle.context();
    if (!_context)
        throw Exception(Status::Failure);

    auto _simulator = simulator->mHandle.get();
    if (!_simulator)
        throw Exception(Status::Failure);

    auto _enableIndirect = (settings->flags & IPL_SIMULATIONFLAGS_REFLECTIONS);
    auto _enablePathing = (settings->flags & IPL_SIMULATIONFLAGS_PATHING);
    auto _sceneType = static_cast<SceneType>(_simulator->sceneType());
    auto _indirectType = static_cast<IndirectEffectType>(_simulator->indirectType());
    auto _maxNumOcclusionSamples = _simulator->maxNumOcclusionSamples();
    auto _maxDuration = _simulator->maxDuration();
    auto _maxOrder = _simulator->maxOrder();
    auto _samplingRate = _simulator->samplingRate();
    auto _frameSize = _simulator->frameSize();
    auto _openCL = _simulator->openCLDevice();
    auto _tan = _simulator->tanDevice();

    new (&mHandle) Handle<SimulationData>(ipl::make_shared<SimulationData>(_enableIndirect, _enablePathing, _sceneType, _indirectType,
                                          _maxNumOcclusionSamples, _maxDuration, _maxOrder,
                                          _samplingRate, _frameSize, _openCL, _tan), _context);
}

ISource* CSource::retain()
{
    mHandle.retain();
    return this;
}

void CSource::release()
{
    if (mHandle.release())
    {
        this->~CSource();
        gMemory().free(this);
    }
}

void CSource::add(ISimulator* simulator)
{
    if (!simulator)
        return;

    auto _simulator = static_cast<CSimulator*>(simulator)->mHandle.get();
    auto _source = mHandle.get();
    if (!_simulator || !_source)
        return;

    _simulator->addSource(_source);
}

void CSource::remove(ISimulator* simulator)
{
    if (!simulator )
        return;

    auto _simulator = static_cast<CSimulator*>(simulator)->mHandle.get();
    auto _source = mHandle.get();
    if (!_simulator || !_source)
        return;

    _simulator->removeSource(_source);
}

void CSource::setInputs(IPLSimulationFlags flags,
                        IPLSimulationInputs* inputs)
{
    if (!inputs)
        return;

    auto _source = mHandle.get();
    if (!_source)
        return;

    if (flags & IPL_SIMULATIONFLAGS_DIRECT)
    {
        _source->directInputs.flags = static_cast<DirectSimulationFlags>(inputs->directFlags);
        _source->directInputs.source = *reinterpret_cast<CoordinateSpace3f*>(&inputs->source);

        switch (inputs->distanceAttenuationModel.type)
        {
        case IPL_DISTANCEATTENUATIONTYPE_DEFAULT:
            _source->directInputs.distanceAttenuationModel = DistanceAttenuationModel{};
            break;
        case IPL_DISTANCEATTENUATIONTYPE_INVERSEDISTANCE:
            _source->directInputs.distanceAttenuationModel = DistanceAttenuationModel(inputs->distanceAttenuationModel.minDistance, nullptr, nullptr);
            break;
        case IPL_DISTANCEATTENUATIONTYPE_CALLBACK:
            _source->directInputs.distanceAttenuationModel = DistanceAttenuationModel(1.0f, inputs->distanceAttenuationModel.callback, inputs->distanceAttenuationModel.userData);
            break;
        }

        switch (inputs->airAbsorptionModel.type)
        {
        case IPL_AIRABSORPTIONTYPE_DEFAULT:
            _source->directInputs.airAbsorptionModel = AirAbsorptionModel{};
            break;
        case IPL_AIRABSORPTIONTYPE_EXPONENTIAL:
            _source->directInputs.airAbsorptionModel = AirAbsorptionModel(inputs->airAbsorptionModel.coefficients, nullptr, nullptr);
            break;
        case IPL_AIRABSORPTIONTYPE_CALLBACK:
            _source->directInputs.airAbsorptionModel = AirAbsorptionModel(nullptr, inputs->airAbsorptionModel.callback, inputs->airAbsorptionModel.userData);
            break;
        }

        _source->directInputs.directivity = *reinterpret_cast<Directivity*>(&inputs->directivity);
        _source->directInputs.occlusionType = static_cast<OcclusionType>(inputs->occlusionType);
        _source->directInputs.occlusionRadius = inputs->occlusionRadius;
        _source->directInputs.numOcclusionSamples = inputs->numOcclusionSamples;

        if (Context::isCallerAPIVersionAtLeast(4, 3))
        {
            _source->directInputs.numTransmissionRays = inputs->numTransmissionRays;
        }
    }

    if (flags & IPL_SIMULATIONFLAGS_REFLECTIONS)
    {
        _source->reflectionInputs.enabled = (inputs->flags & IPL_SIMULATIONFLAGS_REFLECTIONS);
        _source->reflectionInputs.source = *reinterpret_cast<CoordinateSpace3f*>(&inputs->source);

        switch (inputs->distanceAttenuationModel.type)
        {
        case IPL_DISTANCEATTENUATIONTYPE_DEFAULT:
            _source->reflectionInputs.distanceAttenuationModel = DistanceAttenuationModel{};
            break;
        case IPL_DISTANCEATTENUATIONTYPE_INVERSEDISTANCE:
            _source->reflectionInputs.distanceAttenuationModel = DistanceAttenuationModel(inputs->distanceAttenuationModel.minDistance, nullptr, nullptr);
            break;
        case IPL_DISTANCEATTENUATIONTYPE_CALLBACK:
            _source->reflectionInputs.distanceAttenuationModel = DistanceAttenuationModel(1.0f, inputs->distanceAttenuationModel.callback, inputs->distanceAttenuationModel.userData);
            break;
        }

        switch (inputs->airAbsorptionModel.type)
        {
        case IPL_AIRABSORPTIONTYPE_DEFAULT:
            _source->reflectionInputs.airAbsorptionModel = AirAbsorptionModel{};
            break;
        case IPL_AIRABSORPTIONTYPE_EXPONENTIAL:
            _source->reflectionInputs.airAbsorptionModel = AirAbsorptionModel(inputs->airAbsorptionModel.coefficients, nullptr, nullptr);
            break;
        case IPL_AIRABSORPTIONTYPE_CALLBACK:
            _source->reflectionInputs.airAbsorptionModel = AirAbsorptionModel(nullptr, inputs->airAbsorptionModel.callback, inputs->airAbsorptionModel.userData);
            break;
        }

        _source->reflectionInputs.directivity = *reinterpret_cast<Directivity*>(&inputs->directivity);
        _source->reflectionInputs.reverbScale[0] = inputs->reverbScale[0];
        _source->reflectionInputs.reverbScale[1] = inputs->reverbScale[1];
        _source->reflectionInputs.reverbScale[2] = inputs->reverbScale[2];
        _source->reflectionInputs.transitionTime = inputs->hybridReverbTransitionTime;
        _source->reflectionInputs.overlapFraction = inputs->hybridReverbOverlapPercent;
        _source->reflectionInputs.baked = (inputs->baked == IPL_TRUE);
        _source->reflectionInputs.bakedDataIdentifier = *reinterpret_cast<BakedDataIdentifier*>(&inputs->bakedDataIdentifier);
    }

    if (flags & IPL_SIMULATIONFLAGS_PATHING)
    {
        _source->pathingInputs.enabled = (inputs->flags & IPL_SIMULATIONFLAGS_PATHING);
        _source->pathingInputs.source = *reinterpret_cast<CoordinateSpace3f*>(&inputs->source);
        _source->pathingInputs.probes = (inputs->pathingProbes) ? reinterpret_cast<CProbeBatch*>(inputs->pathingProbes)->mHandle.get() : nullptr;
        _source->pathingInputs.visRadius = inputs->visRadius;
        _source->pathingInputs.visThreshold = inputs->visThreshold;
        _source->pathingInputs.visRange = inputs->visRange;
        _source->pathingInputs.order = inputs->pathingOrder;
        _source->pathingInputs.enableValidation = (inputs->enableValidation == IPL_TRUE);
        _source->pathingInputs.findAlternatePaths = (inputs->findAlternatePaths == IPL_TRUE);
        _source->pathingInputs.simplifyPaths = true;
        _source->pathingInputs.realTimeVis = true;
    }
}

void CSource::getOutputs(IPLSimulationFlags flags,
                         IPLSimulationOutputs* outputs)
{
    if (!outputs)
        return;

    auto _source = mHandle.get();
    if (!_source)
        return;

    if (flags & IPL_SIMULATIONFLAGS_DIRECT)
    {
        outputs->direct.distanceAttenuation = _source->directOutputs.directPath.distanceAttenuation;
        outputs->direct.airAbsorption[0] = _source->directOutputs.directPath.airAbsorption[0];
        outputs->direct.airAbsorption[1] = _source->directOutputs.directPath.airAbsorption[1];
        outputs->direct.airAbsorption[2] = _source->directOutputs.directPath.airAbsorption[2];
        outputs->direct.directivity = _source->directOutputs.directPath.directivity;
        outputs->direct.occlusion = _source->directOutputs.directPath.occlusion;
        outputs->direct.transmission[0] = _source->directOutputs.directPath.transmission[0];
        outputs->direct.transmission[1] = _source->directOutputs.directPath.transmission[1];
        outputs->direct.transmission[2] = _source->directOutputs.directPath.transmission[2];
    }

    if (flags & IPL_SIMULATIONFLAGS_REFLECTIONS)
    {
        outputs->reflections.ir = reinterpret_cast<IPLReflectionEffectIR>(&_source->reflectionOutputs.overlapSaveFIR);
        outputs->reflections.numChannels = _source->reflectionOutputs.numChannels;
        outputs->reflections.irSize = _source->reflectionOutputs.numSamples;
        outputs->reflections.reverbTimes[0] = _source->reflectionOutputs.reverb.reverbTimes[0];
        outputs->reflections.reverbTimes[1] = _source->reflectionOutputs.reverb.reverbTimes[1];
        outputs->reflections.reverbTimes[2] = _source->reflectionOutputs.reverb.reverbTimes[2];
        outputs->reflections.eq[0] = _source->reflectionOutputs.hybridEQ[0];
        outputs->reflections.eq[1] = _source->reflectionOutputs.hybridEQ[1];
        outputs->reflections.eq[2] = _source->reflectionOutputs.hybridEQ[2];
        outputs->reflections.delay = _source->reflectionOutputs.hybridDelay;
        outputs->reflections.tanSlot = _source->reflectionOutputs.tanSlot;
    }

    if (flags & IPL_SIMULATIONFLAGS_PATHING)
    {
        memcpy(outputs->pathing.eqCoeffs, _source->pathingOutputs.eq, 3 * sizeof(float));
        outputs->pathing.shCoeffs = _source->pathingOutputs.sh.data();
    }
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createSimulator(IPLSimulationSettings* settings,
                                   ISimulator** simulator)
{
    if (!settings || !simulator)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _simulator = reinterpret_cast<CSimulator*>(gMemory().allocate(sizeof(CSimulator), Memory::kDefaultAlignment));
        new (_simulator) CSimulator(this, settings);
        *simulator = _simulator;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
