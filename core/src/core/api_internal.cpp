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

#include "containers.h"
#include "direct_simulator.h"
#include "energy_field_factory.h"
#include "hrtf_database.h"
#include "hybrid_reverb_estimator.h"
#include "impulse_response_factory.h"
#include "opencl_energy_field.h"
#include "opencl_impulse_response.h"
#include "path_simulator.h"
#include "profiler.h"
#include "reconstructor_factory.h"
#include "reflection_simulator_factory.h"
#include "sh.h"
#include "simulation_data.h"
#include "simulation_manager.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_opencl_device.h"
#include "api_radeonrays_device.h"
#include "api_tan_device.h"
#include "api_scene.h"
#include "api_probes.h"
#include "api_simulator.h"

#include "phonon_internal.h"

// --------------------------------------------------------------------------------------------------------------------
// Context
// --------------------------------------------------------------------------------------------------------------------

void IPLCALL iplContextSetVariableBool(IPLContext context, IPLstring name, IPLbool value)
{
    if (!context)
        return;

    if (string(name) == "enable_dc_correction_for_phase_interpolation")
    {
        HRTFDatabase::sEnableDCCorrectionForPhaseInterpolation = value;
    }
    else if (string(name) == "enable_paths_from_all_source_probes")
    {
        PathSimulator::sEnablePathsFromAllSourceProbes = value;
    }
}

void IPLCALL iplContextSetVariableInt32(IPLContext context, IPLstring name, IPLint32 value)
{
    if (!context)
        return;
}

void IPLCALL iplContextSetVariableFloat32(IPLContext context, IPLstring name, IPLfloat32 value)
{
    if (!context)
        return;

    if (string(name) == "max_hrtf_normalization_volume_gain_db")
    {
        Loudness::sMaxVolumeNormGainDB = value;
    }
}

void IPLCALL iplContextSetVariableString(IPLContext context, IPLstring name, IPLstring value)
{
    if (!context)
        return;
}

void IPLCALL iplContextSetProfilerContext(IPLContext context, void* profilerContext)
{
    if (!context)
        return;

    reinterpret_cast<api::CContext*>(context)->setProfilerContext(profilerContext);
}


// --------------------------------------------------------------------------------------------------------------------
// Distance Attenuation
// --------------------------------------------------------------------------------------------------------------------

void IPLCALL iplDistanceAttenuationGetCorrectionCurve(IPLDistanceAttenuationModel* model, IPLint32 numSamples, IPLint32 samplingRate, IPLfloat32* correctionCurve)
{
    if (!model || numSamples <= 0 || samplingRate <= 0 || !correctionCurve)
        return;

    DistanceAttenuationModel _model{};
    switch (model->type)
    {
    case IPL_DISTANCEATTENUATIONTYPE_DEFAULT:
        _model = DistanceAttenuationModel{};
        break;
    case IPL_DISTANCEATTENUATIONTYPE_INVERSEDISTANCE:
        _model = DistanceAttenuationModel(model->minDistance, nullptr, nullptr);
        break;
    case IPL_DISTANCEATTENUATIONTYPE_CALLBACK:
        _model = DistanceAttenuationModel(1.0f, model->callback, model->userData);
        break;
    }

    _model.generateCorrectionCurve(DistanceAttenuationModel{}, _model, samplingRate, numSamples, correctionCurve);
}


// --------------------------------------------------------------------------------------------------------------------
// Direct Simulation
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLDirectSimulator, DirectSimulator);

IPLerror IPLCALL iplDirectSimulatorCreate(IPLContext context, IPLDirectSimulatorSettings* settings, IPLDirectSimulator* simulator)
{
    if (!context || !settings || !simulator)
        return IPL_STATUS_FAILURE;

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *simulator = createHandle(_context, ipl::make_shared<DirectSimulator>(settings->maxNumOcclusionSamples));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLDirectSimulator IPLCALL iplDirectSimulatorRetain(IPLDirectSimulator simulator)
{
    return retainHandle<DirectSimulator>(simulator);
}

void IPLCALL iplDirectSimulatorRelease(IPLDirectSimulator* simulator)
{
    releaseHandle<DirectSimulator>(*simulator);
}

void IPLCALL iplDirectSimulatorSimulate(IPLDirectSimulator simulator, IPLScene scene, IPLDirectSimulatorParams* inputs, IPLDirectEffectParams* outputs)
{
    if (!simulator || !scene || !inputs || !outputs)
        return;

    auto _simulator = derefHandle<DirectSimulator>(simulator);
    auto _scene = reinterpret_cast<api::CScene*>(scene)->mHandle.get();
    if (!_simulator || !_scene)
        return;

    auto _flags = static_cast<DirectSimulationFlags>(inputs->flags);
    auto _source = *reinterpret_cast<CoordinateSpace3f*>(&inputs->source);
    auto _listener = *reinterpret_cast<CoordinateSpace3f*>(&inputs->listener);
    auto _directivity = *reinterpret_cast<Directivity*>(&inputs->directivity);
    auto _occlusionType = static_cast<OcclusionType>(inputs->occlusionType);

    DistanceAttenuationModel _distanceAttenuationModel{};
    switch (inputs->distanceAttenuationModel.type)
    {
    case IPL_DISTANCEATTENUATIONTYPE_DEFAULT:
        _distanceAttenuationModel = DistanceAttenuationModel{};
        break;
    case IPL_DISTANCEATTENUATIONTYPE_INVERSEDISTANCE:
        _distanceAttenuationModel = DistanceAttenuationModel(inputs->distanceAttenuationModel.minDistance, nullptr, nullptr);
        break;
    case IPL_DISTANCEATTENUATIONTYPE_CALLBACK:
        _distanceAttenuationModel = DistanceAttenuationModel(1.0f, inputs->distanceAttenuationModel.callback, inputs->distanceAttenuationModel.userData);
        break;
    }

    AirAbsorptionModel _airAbsorptionModel{};
    switch (inputs->airAbsorptionModel.type)
    {
    case IPL_AIRABSORPTIONTYPE_DEFAULT:
        _airAbsorptionModel = AirAbsorptionModel{};
        break;
    case IPL_AIRABSORPTIONTYPE_EXPONENTIAL:
        _airAbsorptionModel = AirAbsorptionModel(inputs->airAbsorptionModel.coefficients, nullptr, nullptr);
        break;
    case IPL_AIRABSORPTIONTYPE_CALLBACK:
        _airAbsorptionModel = AirAbsorptionModel(nullptr, inputs->airAbsorptionModel.callback, inputs->airAbsorptionModel.userData);
        break;
    }

    DirectSoundPath _directPath{};

    _simulator->simulate(_scene.get(), _flags, _source, _listener, _distanceAttenuationModel, _airAbsorptionModel,
        _directivity, _occlusionType, inputs->occlusionRadius, inputs->numOcclusionSamples,
        inputs->numTransmissionRays, _directPath);

    outputs->distanceAttenuation = _directPath.distanceAttenuation;
    outputs->airAbsorption[0] = _directPath.airAbsorption[0];
    outputs->airAbsorption[1] = _directPath.airAbsorption[1];
    outputs->airAbsorption[2] = _directPath.airAbsorption[2];
    outputs->directivity = _directPath.directivity;
    outputs->occlusion = _directPath.occlusion;
    outputs->transmission[0] = _directPath.transmission[0];
    outputs->transmission[1] = _directPath.transmission[1];
    outputs->transmission[2] = _directPath.transmission[2];
}


// --------------------------------------------------------------------------------------------------------------------
// Energy Field
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLEnergyField, EnergyField);

IPLerror IPLCALL iplEnergyFieldCreate(IPLContext context, IPLEnergyFieldSettings* settings, IPLEnergyField* energyField)
{
    if (!context || !settings || !energyField)
        return IPL_STATUS_FAILURE;

    auto _sceneType = static_cast<SceneType>(settings->sceneType);
    auto _openCLDevice = reinterpret_cast<api::COpenCLDevice*>(settings->openCLDevice)->mHandle.get();

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *energyField = createHandle(_context, ipl::shared_ptr<EnergyField>(EnergyFieldFactory::create(_sceneType,
            settings->duration, settings->order, _openCLDevice)));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLEnergyField IPLCALL iplEnergyFieldRetain(IPLEnergyField energyField)
{
    return retainHandle<EnergyField>(energyField);
}

void IPLCALL iplEnergyFieldRelease(IPLEnergyField* energyField)
{
    releaseHandle<EnergyField>(*energyField);
}

IPLint32 IPLCALL iplEnergyFieldGetNumChannels(IPLEnergyField energyField)
{
    if (!energyField)
        return 0;

    auto _energyField = derefHandle<EnergyField>(energyField);
    if (!_energyField)
        return 0;

    return _energyField->numChannels();
}

IPLint32 IPLCALL iplEnergyFieldGetNumBins(IPLEnergyField energyField)
{
    if (!energyField)
        return 0;

    auto _energyField = derefHandle<EnergyField>(energyField);
    if (!_energyField)
        return 0;

    return _energyField->numBins();
}

IPLint32 IPLCALL iplEnergyFieldGetSize(IPLEnergyField energyField)
{
    if (!energyField)
        return 0;

    auto _energyField = derefHandle<EnergyField>(energyField);
    if (!_energyField)
        return 0;

    return _energyField->numChannels() * Bands::kNumBands * _energyField->numBins();
}

IPLfloat32* IPLCALL iplEnergyFieldGetData(IPLEnergyField energyField)
{
    if (!energyField)
        return nullptr;

    auto _energyField = derefHandle<EnergyField>(energyField);
    if (!_energyField)
        return nullptr;

    return _energyField->flatData();
}

void IPLCALL iplEnergyFieldSetData(IPLEnergyField energyField, IPLfloat32* data)
{
    if (!energyField)
        return;

    auto _energyField = derefHandle<EnergyField>(energyField);
    if (!_energyField)
        return;

    auto size = _energyField->numChannels() * Bands::kNumBands * _energyField->numBins();
    memcpy(_energyField->flatData(), data, size * sizeof(float));
}

void IPLCALL iplEnergyFieldCopyHostToDevice(IPLEnergyField energyField)
{
    if (!energyField)
        return;

    auto _energyField = derefHandle<EnergyField>(energyField);
    if (!_energyField)
        return;

#if defined(IPL_USES_RADEONRAYS)
    static_cast<OpenCLEnergyField*>(_energyField.get())->copyHostToDevice();
#endif
}

void IPLCALL iplEnergyFieldCopyDeviceToHost(IPLEnergyField energyField)
{
    if (!energyField)
        return;

    auto _energyField = derefHandle<EnergyField>(energyField);
    if (!_energyField)
        return;

#if defined(IPL_USES_RADEONRAYS)
    static_cast<OpenCLEnergyField*>(_energyField.get())->copyDeviceToHost();
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// Impulse Response
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLImpulseResponse, ImpulseResponse);

IPLerror IPLCALL iplImpulseResponseCreate(IPLContext context, IPLImpulseResponseSettings* settings, IPLImpulseResponse* impulseResponse)
{
    if (!context || !settings || !impulseResponse)
        return IPL_STATUS_FAILURE;

    auto _indirectType = static_cast<IndirectEffectType>(settings->indirectType);
    auto _openCLDevice = reinterpret_cast<api::COpenCLDevice*>(settings->openCLDevice)->mHandle.get();

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *impulseResponse = createHandle(_context, ipl::shared_ptr<ImpulseResponse>(ImpulseResponseFactory::create(_indirectType,
            settings->duration, settings->order, settings->samplingRate, _openCLDevice)));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLImpulseResponse IPLCALL iplImpulseResponseRetain(IPLImpulseResponse impulseResponse)
{
    return retainHandle<ImpulseResponse>(impulseResponse);
}

void IPLCALL iplImpulseResponseRelease(IPLImpulseResponse* impulseResponse)
{
    releaseHandle<ImpulseResponse>(*impulseResponse);
}

IPLint32 IPLCALL iplImpulseResponseGetNumChannels(IPLImpulseResponse impulseResponse)
{
    if (!impulseResponse)
        return 0;

    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_impulseResponse)
        return 0;

    return _impulseResponse->numChannels();
}

IPLint32 IPLCALL iplImpulseResponseGetNumSamples(IPLImpulseResponse impulseResponse)
{
    if (!impulseResponse)
        return 0;

    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_impulseResponse)
        return 0;

    return _impulseResponse->numSamples();
}

IPLint32 IPLCALL iplImpulseResponseGetSize(IPLImpulseResponse impulseResponse)
{
    if (!impulseResponse)
        return 0;

    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_impulseResponse)
        return 0;

    return _impulseResponse->numChannels() * _impulseResponse->numSamples();
}

IPLfloat32* IPLCALL iplImpulseResponseGetData(IPLImpulseResponse impulseResponse)
{
    if (!impulseResponse)
        return nullptr;

    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_impulseResponse)
        return nullptr;

    return _impulseResponse->data();
}

void IPLCALL iplImpulseResponseSetData(IPLImpulseResponse impulseResponse, IPLfloat32* data)
{
    if (!impulseResponse)
        return;

    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_impulseResponse)
        return;

    auto size = _impulseResponse->numChannels() * _impulseResponse->numSamples();
    memcpy(_impulseResponse->data(), data, size * sizeof(float));
}

void IPLCALL iplImpulseResponseCopyHostToDevice(IPLImpulseResponse impulseResponse)
{
    if (!impulseResponse)
        return;

    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_impulseResponse)
        return;

#if defined(IPL_USES_OPENCL)
    static_cast<OpenCLImpulseResponse*>(_impulseResponse.get())->copyHostToDevice();
#endif
}

void IPLCALL iplImpulseResponseCopyDeviceToHost(IPLImpulseResponse impulseResponse)
{
    if (!impulseResponse)
        return;

    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_impulseResponse)
        return;

#if defined(IPL_USES_OPENCL)
    static_cast<OpenCLImpulseResponse*>(_impulseResponse.get())->copyDeviceToHost();
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// Indirect Effect IR
// --------------------------------------------------------------------------------------------------------------------

IPLerror IPLCALL iplIndirectEffectIRCreate(IPLContext context, IPLIndirectEffectIRSettings* settings, IPLReflectionEffectIR* ir)
{
    if (!context || !settings || !ir)
        return IPL_STATUS_FAILURE;

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _ir = ipl::make_shared<TripleBuffer<OverlapSaveFIR>>();

        auto numChannels = SphericalHarmonics::numCoeffsForOrder(settings->order);
        auto irSize = static_cast<int>(ceilf(settings->duration * settings->samplingRate));

        _ir->initBuffers(numChannels, irSize, settings->frameSize);

        *ir = createHandle(_context, _ir);
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLReflectionEffectIR IPLCALL iplIndirectEffectIRRetain(IPLReflectionEffectIR ir)
{
    return retainHandle<TripleBuffer<OverlapSaveFIR>>(ir);
}

void IPLCALL iplIndirectEffectIRRelease(IPLReflectionEffectIR* ir)
{
    releaseHandle<TripleBuffer<OverlapSaveFIR>>(*ir);
}


// --------------------------------------------------------------------------------------------------------------------
// Reflection Simulator
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLReflectionSimulator, IReflectionSimulator);

IPLerror IPLCALL iplReflectionSimulatorCreate(IPLContext context, IPLReflectionSimulatorSettings* settings, IPLReflectionSimulator* simulator)
{
    if (!context || !settings || !simulator)
        return IPL_STATUS_FAILURE;

    auto _sceneType = static_cast<SceneType>(settings->sceneType);
    auto _radeonRaysDevice = reinterpret_cast<api::CRadeonRaysDevice*>(settings->radeonRaysDevice)->mHandle.get();

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *simulator = createHandle(_context, ipl::shared_ptr<IReflectionSimulator>(ReflectionSimulatorFactory::create(_sceneType,
            settings->maxNumRays, settings->numDiffuseSamples, settings->maxDuration,
            settings->maxOrder, settings->maxNumSources, settings->maxNumListeners,
            settings->numThreads, settings->rayBatchSize, _radeonRaysDevice)));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLReflectionSimulator IPLCALL iplReflectionSimulatorRetain(IPLReflectionSimulator simulator)
{
    return retainHandle<IReflectionSimulator>(simulator);
}

void IPLCALL iplReflectionSimulatorRelease(IPLReflectionSimulator* simulator)
{
    releaseHandle<IReflectionSimulator>(*simulator);
}

void IPLCALL iplReflectionSimulatorSimulate(IPLReflectionSimulator simulator, IPLScene scene, IPLReflectionSimulatorParams* inputs, IPLReflectionSimulatorOutputs* outputs)
{
    if (!simulator || !scene || !inputs || !outputs)
        return;

    auto _simulator = derefHandle<IReflectionSimulator>(simulator);
    auto _scene = reinterpret_cast<api::CScene*>(scene)->mHandle.get();
    if (!_simulator || !_scene)
        return;

    auto _sources = reinterpret_cast<CoordinateSpace3f*>(inputs->sources);
    auto _listeners = reinterpret_cast<CoordinateSpace3f*>(inputs->listeners);
    auto _directivities = reinterpret_cast<Directivity*>(inputs->directivities);

    // FIXME: Shouldn't have to allocate here.
    auto numEnergyFields = std::max(inputs->numSources, inputs->numListeners);
    Array<EnergyField*> _energyFields(numEnergyFields);
    for (auto i = 0; i < numEnergyFields; ++i)
    {
        _energyFields[i] = derefHandle<EnergyField>(outputs->energyFields[i]).get();
    }

    JobGraph jobGraph;

    _simulator->simulate(*_scene, inputs->numSources, _sources, inputs->numListeners, _listeners, _directivities,
        inputs->numRays, inputs->numBounces, inputs->duration, inputs->order,
        inputs->irradianceMinDistance, _energyFields.data(), jobGraph);

    // FIXME: Shouldn't have to recreate the thread pool here, ThreadPool should be exposed in the API.
    ThreadPool threadPool(inputs->numThreads);
    threadPool.process(jobGraph);
}


// --------------------------------------------------------------------------------------------------------------------
// Reconstructor
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLReconstructor, IReconstructor);

IPLerror IPLCALL iplReconstructorCreate(IPLContext context, IPLReconstructorSettings* settings, IPLReconstructor* reconstructor)
{
    if (!context || !settings || !reconstructor)
        return IPL_STATUS_FAILURE;

    auto _sceneType = static_cast<SceneType>(settings->sceneType);
    auto _indirectType = static_cast<IndirectEffectType>(settings->indirectType);
    auto _radeonRaysDevice = reinterpret_cast<api::CRadeonRaysDevice*>(settings->radeonRaysDevice)->mHandle.get();

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *reconstructor = createHandle(_context, ipl::shared_ptr<IReconstructor>(ReconstructorFactory::create(_sceneType,
            _indirectType, settings->maxDuration, settings->maxOrder, settings->samplingRate,
            _radeonRaysDevice)));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLReconstructor IPLCALL iplReconstructorRetain(IPLReconstructor reconstructor)
{
    return retainHandle<IReconstructor>(reconstructor);
}

void IPLCALL iplReconstructorRelease(IPLReconstructor* reconstructor)
{
    releaseHandle<IReconstructor>(*reconstructor);
}

void IPLCALL iplReconstructorReconstruct(IPLReconstructor reconstructor, IPLReconstructorParams* inputs, IPLReconstructorOutputs* outputs)
{
    if (!reconstructor || !inputs || !outputs)
        return;

    auto _reconstructor = derefHandle<IReconstructor>(reconstructor);
    if (!_reconstructor)
        return;

    auto _reconstructionType = ReconstructionType::Linear;

    // FIXME: Shouldn't have to allocate here.
    Array<EnergyField*> _energyFields(inputs->numIRs);
    for (auto i = 0; i < inputs->numIRs; ++i)
    {
        _energyFields[i] = derefHandle<EnergyField>(inputs->energyFields[i]).get();
    }

    // FIXME: Shouldn't have to allocate here.
    Array<AirAbsorptionModel> _airAbsorptionModels(inputs->numIRs);
    for (auto i = 0; i < inputs->numIRs; ++i)
    {
        switch (inputs->airAbsorptionModels[i].type)
        {
        case IPL_AIRABSORPTIONTYPE_DEFAULT:
            _airAbsorptionModels[i] = AirAbsorptionModel{};
            break;
        case IPL_AIRABSORPTIONTYPE_EXPONENTIAL:
            _airAbsorptionModels[i] = AirAbsorptionModel(inputs->airAbsorptionModels[i].coefficients, nullptr, nullptr);
            break;
        case IPL_AIRABSORPTIONTYPE_CALLBACK:
            _airAbsorptionModels[i] = AirAbsorptionModel(nullptr, inputs->airAbsorptionModels[i].callback, inputs->airAbsorptionModels[i].userData);
            break;
        }
    }

    // FIXME: Shouldn't have to allocate here.
    Array<ImpulseResponse*> _impulseResponses(inputs->numIRs);
    for (auto i = 0; i < inputs->numIRs; ++i)
    {
        _impulseResponses[i] = derefHandle<ImpulseResponse>(outputs->impulseResponses[i]).get();
    }

    _reconstructor->reconstruct(inputs->numIRs, _energyFields.data(), inputs->distanceAttenuationCorrectionCurves,
        _airAbsorptionModels.data(), _impulseResponses.data(), _reconstructionType,
        inputs->duration, inputs->order);
}


// --------------------------------------------------------------------------------------------------------------------
// Reverb Estimator
// --------------------------------------------------------------------------------------------------------------------

void IPLCALL iplReverbEstimatorEstimate(IPLReverbEstimatorParams* inputs, IPLReverbEstimatorOutputs* outputs)
{
    if (!inputs || !outputs)
        return;

    auto _energyField = derefHandle<EnergyField>(inputs->energyField);
    if (!_energyField)
        return;

    AirAbsorptionModel _airAbsorptionModel{};
    switch (inputs->airAbsorptionModel.type)
    {
    case IPL_AIRABSORPTIONTYPE_DEFAULT:
        _airAbsorptionModel = AirAbsorptionModel{};
        break;
    case IPL_AIRABSORPTIONTYPE_EXPONENTIAL:
        _airAbsorptionModel = AirAbsorptionModel(inputs->airAbsorptionModel.coefficients, nullptr, nullptr);
        break;
    case IPL_AIRABSORPTIONTYPE_CALLBACK:
        _airAbsorptionModel = AirAbsorptionModel(nullptr, inputs->airAbsorptionModel.callback, inputs->airAbsorptionModel.userData);
        break;
    }

    Reverb _reverb;
    ReverbEstimator::estimate(*_energyField, _airAbsorptionModel, _reverb);

    outputs->reverbTimes[0] = _reverb.reverbTimes[0];
    outputs->reverbTimes[1] = _reverb.reverbTimes[1];
    outputs->reverbTimes[2] = _reverb.reverbTimes[2];
}


// --------------------------------------------------------------------------------------------------------------------
// Hybrid Reverb Estimator
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLHybridReverbEstimator, HybridReverbEstimator);

IPLerror IPLCALL iplHybridReverbEstimatorCreate(IPLContext context, IPLHybridReverbEstimatorSettings* settings, IPLHybridReverbEstimator* estimator)
{
    if (!context || !settings || !estimator)
        return IPL_STATUS_FAILURE;

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *estimator = createHandle(_context, ipl::make_shared<HybridReverbEstimator>(settings->maxDuration, settings->samplingRate,
            settings->frameSize));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLHybridReverbEstimator IPLCALL iplHybridReverbEstimatorRetain(IPLHybridReverbEstimator estimator)
{
    return retainHandle<HybridReverbEstimator>(estimator);
}

void IPLCALL iplHybridReverbEstimatorRelease(IPLHybridReverbEstimator* estimator)
{
    releaseHandle<HybridReverbEstimator>(*estimator);
}

void IPLCALL iplHybridReverbEstimatorEstimate(IPLHybridReverbEstimator estimator, IPLHybridReverbEstimatorParams* inputs, IPLHybridReverbEstimatorOutputs* outputs)
{
    if (!estimator || !inputs || !outputs)
        return;

    auto _estimator = derefHandle<HybridReverbEstimator>(estimator);
    auto _energyField = derefHandle<EnergyField>(inputs->energyField);
    auto _impulseResponse = derefHandle<ImpulseResponse>(inputs->impulseResponse);
    if (!_estimator || !_energyField || _impulseResponse)
        return;

    Reverb _reverb;
    _reverb.reverbTimes[0] = inputs->reverbTimes[0];
    _reverb.reverbTimes[1] = inputs->reverbTimes[1];
    _reverb.reverbTimes[2] = inputs->reverbTimes[2];

    _estimator->estimate(*_energyField, _reverb, *_impulseResponse, inputs->transitionTime, inputs->overlapFraction,
        inputs->order, outputs->eq);
}


// --------------------------------------------------------------------------------------------------------------------
// Convolution Partitioner
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLConvolutionPartitioner, OverlapSavePartitioner);

IPLerror IPLCALL iplConvolutionPartitionerCreate(IPLContext context, IPLConvolutionPartitionerSettings* settings, IPLConvolutionPartitioner* partitioner)
{
    if (!context || !settings || !partitioner)
        return IPL_STATUS_FAILURE;

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *partitioner = createHandle(_context, ipl::make_shared<OverlapSavePartitioner>(settings->frameSize));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLConvolutionPartitioner IPLCALL iplConvolutionPartitionerRetain(IPLConvolutionPartitioner partitioner)
{
    return retainHandle<OverlapSavePartitioner>(partitioner);
}

void IPLCALL iplConvolutionPartitionerRelease(IPLConvolutionPartitioner* partitioner)
{
    releaseHandle<OverlapSavePartitioner>(*partitioner);
}

void IPLCALL iplConvolutionPartitionerPartition(IPLConvolutionPartitioner partitioner, IPLConvolutionPartitionerParams* inputs, IPLConvolutionPartitionerOutputs* outputs)
{
    if (!partitioner || !inputs || !outputs)
        return;

    auto _partitioner = derefHandle<OverlapSavePartitioner>(partitioner);
    auto _impulseResponse = derefHandle<ImpulseResponse>(inputs->impulseResponse);
    auto _ir = derefHandle<TripleBuffer<OverlapSaveFIR>>(outputs->ir);
    if (!_partitioner || !_impulseResponse || !_ir)
        return;

    auto numChannels = SphericalHarmonics::numCoeffsForOrder(inputs->order);
    auto numSamples = static_cast<int>(ceilf(inputs->duration * inputs->samplingRate));

    _partitioner->partition(*_impulseResponse, numChannels, numSamples, *_ir->writeBuffer);
    _ir->commitWriteBuffer();
}


// --------------------------------------------------------------------------------------------------------------------
// TrueAudio Next
// --------------------------------------------------------------------------------------------------------------------

IPLint32 IPLCALL iplTrueAudioNextDeviceAcquireSlot(IPLTrueAudioNextDevice tanDevice)
{
    if (!tanDevice)
        return -1;

    auto _tanDevice = reinterpret_cast<api::CTrueAudioNextDevice*>(tanDevice)->mHandle.get();
    if (!_tanDevice)
        return -1;

#if defined(IPL_USES_TRUEAUDIONEXT)
    return _tanDevice->acquireSlot();
#else
    return -1;
#endif
}

void IPLCALL iplTrueAudioNextReleaseSlot(IPLTrueAudioNextDevice tanDevice, IPLint32 slot)
{
    if (!tanDevice)
        return;

    auto _tanDevice = reinterpret_cast<api::CTrueAudioNextDevice*>(tanDevice)->mHandle.get();
    if (!_tanDevice)
        return;

#if defined(IPL_USES_TRUEAUDIONEXT)
    _tanDevice->releaseSlot(slot);
#endif
}

void IPLCALL iplTrueAudioNextSetImpulseResponse(IPLTrueAudioNextDevice tanDevice, IPLint32 slot, IPLImpulseResponse impulseResponse)
{
    if (!tanDevice || slot < 0 || !impulseResponse)
        return;

    auto _tanDevice = reinterpret_cast<api::CTrueAudioNextDevice*>(tanDevice)->mHandle.get();
    auto _impulseResponse = derefHandle<ImpulseResponse>(impulseResponse);
    if (!_tanDevice || !_impulseResponse)
        return;

#if defined(IPL_USES_TRUEAUDIONEXT)
    _tanDevice->setIR(slot, static_cast<OpenCLImpulseResponse*>(_impulseResponse.get())->channelBuffers());
#endif
}

void IPLCALL iplTrueAudioNextUpdateIRs(IPLTrueAudioNextDevice tanDevice)
{
    if (!tanDevice)
        return;

    auto _tanDevice = reinterpret_cast<api::CTrueAudioNextDevice*>(tanDevice)->mHandle.get();
    if (!_tanDevice)
        return;

#if defined(IPL_USES_TRUEAUDIONEXT)
    _tanDevice->updateIRs();
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// Probes
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLProbeNeighborhood, ProbeNeighborhood);

namespace api {

class CProbeNeighborhood
{
public:
    Handle<ProbeNeighborhood> mHandle;

    CProbeNeighborhood(CContext* context)
    {
        auto _context = context->mHandle.get();
        if (!_context)
            throw Exception(Status::Failure);

        new (&mHandle) Handle<ProbeNeighborhood>(ipl::make_shared<ProbeNeighborhood>(), _context);
    }

    CProbeNeighborhood* retain()
    {
        mHandle.retain();
        return this;
    }

    void release()
    {
        if (mHandle.release())
        {
            this->~CProbeNeighborhood();
            gMemory().free(this);
        }
    }

    void resize(IPLint32 maxProbes)
    {
        auto _probeNeighborhood = mHandle.get();
        if (!_probeNeighborhood)
            return;

        _probeNeighborhood->resize(maxProbes);
    }

    void reset()
    {
        auto _probeNeighborhood = mHandle.get();
        if (!_probeNeighborhood)
            return;

        _probeNeighborhood->reset();
    }

    IPLint32 numProbes()
    {
        auto _probeNeighborhood = mHandle.get();
        if (!_probeNeighborhood)
            return 0;

        return _probeNeighborhood->numProbes();
    }

    IPLint32 numValidProbes()
    {
        auto _probeNeighborhood = mHandle.get();
        if (!_probeNeighborhood)
            return 0;

        return _probeNeighborhood->numValidProbes();
    }

    IPLint32 findNearest(IPLVector3 position)
    {
        auto _probeNeighborhood = mHandle.get();
        if (!_probeNeighborhood)
            return 0;

        const auto& _position = *reinterpret_cast<Vector3f*>(&position);
        return _probeNeighborhood->findNearest(_position);
    }

    void checkOcclusion(IScene* scene, IPLVector3 position)
    {
        auto _probeNeighborhood = mHandle.get();
        auto _scene = reinterpret_cast<CScene*>(scene)->mHandle.get();

        if (!_probeNeighborhood || !_scene)
            return;

        const auto& _position = *reinterpret_cast<Vector3f*>(&position);
        _probeNeighborhood->checkOcclusion(*_scene, _position);
    }

    void calcWeights(IPLVector3 position)
    {
        auto _probeNeighborhood = mHandle.get();
        if (!_probeNeighborhood)
            return;

        const auto& _position = *reinterpret_cast<Vector3f*>(&position);
        _probeNeighborhood->calcWeights(_position);
    }
};

}

IPLerror IPLCALL iplProbeNeighborhoodCreate(IPLContext context, IPLProbeNeighborhood* probeNeighborhood)
{
    if (!context)
        return IPL_STATUS_FAILURE;

    if (!probeNeighborhood)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _probeNeighborhood = reinterpret_cast<api::CProbeNeighborhood*>(gMemory().allocate(sizeof(api::CProbeNeighborhood), Memory::kDefaultAlignment));
        new (_probeNeighborhood) api::CProbeNeighborhood(reinterpret_cast<api::CContext*>(context));
        *probeNeighborhood = reinterpret_cast<IPLProbeNeighborhood>(_probeNeighborhood);
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

void IPLCALL iplProbeNeighborhoodRelease(IPLProbeNeighborhood* probeNeighborhood)
{
    if (!probeNeighborhood || !*probeNeighborhood)
        return;

    reinterpret_cast<api::CProbeNeighborhood*>(*probeNeighborhood)->release();

    *probeNeighborhood = nullptr;
}

IPLProbeNeighborhood IPLCALL iplProbeNeighborhoodRetain(IPLProbeNeighborhood probeNeighborhood)
{
    if (!probeNeighborhood)
        return nullptr;

    return reinterpret_cast<IPLProbeNeighborhood>(reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood)->retain());
}

void IPLCALL iplProbeNeighborhoodResize(IPLProbeNeighborhood probeNeighborhood, IPLint32 maxProbes)
{
    if (!probeNeighborhood)
        return;

    reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood)->resize(maxProbes);
}

void IPLCALL iplProbeNeighborhoodReset(IPLProbeNeighborhood probeNeighborhood)
{
    if (!probeNeighborhood)
        return;

    reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood)->reset();
}

IPLint32 IPLCALL iplProbeNeighborhoodGetNumProbes(IPLProbeNeighborhood probeNeighborhood)
{
    if (!probeNeighborhood)
        return 0;

    return reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood)->numProbes();
}

IPLint32 IPLCALL iplProbeNeighborhoodGetNumValidProbes(IPLProbeNeighborhood probeNeighborhood)
{
    if (!probeNeighborhood)
        return 0;

    return reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood)->numValidProbes();
}

IPLint32 IPLCALL iplProbeNeighborhoodFindNearest(IPLProbeNeighborhood probeNeighborhood, IPLVector3 point)
{
    if (!probeNeighborhood)
        return -1;

    return reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood)->findNearest(point);
}

void IPLCALL iplProbeNeighborhoodCheckOcclusion(IPLProbeNeighborhood probeNeighborhood, IPLScene scene, IPLVector3 point)
{
    if (!probeNeighborhood || !scene)
        return;

    api::CProbeNeighborhood* _probeNeighborhood = reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood);
    api::IScene* _scene = reinterpret_cast<api::IScene*>(scene);

    _probeNeighborhood->checkOcclusion(_scene, point);
}

void IPLCALL iplProbeNeighborhoodCalculateWeights(IPLProbeNeighborhood probeNeighborhood, IPLVector3 point)
{
    if (!probeNeighborhood)
        return;

    api::CProbeNeighborhood* _probeNeighborhood = reinterpret_cast<api::CProbeNeighborhood*>(probeNeighborhood);

    _probeNeighborhood->calcWeights(point);
}

void IPLCALL iplProbeBatchUpdateProbeRadius(IPLProbeBatch probeBatch, IPLint32 index, IPLfloat32 radius)
{
    if (!probeBatch || index < 0 || radius < 0.0f)
        return;

    auto _probeBatch = reinterpret_cast<api::CProbeBatch*>(probeBatch)->mHandle.get();
    if (!_probeBatch)
        return;

    if (index < 0 || _probeBatch->numProbes() <= index)
        return;

    _probeBatch->updateProbeRadius(index, radius);
}

void IPLCALL iplProbeBatchUpdateProbePosition(IPLProbeBatch probeBatch, IPLint32 index, IPLVector3 position)
{
    if (!probeBatch || index < 0)
        return;

    auto _probeBatch = reinterpret_cast<api::CProbeBatch*>(probeBatch)->mHandle.get();
    if (!_probeBatch)
        return;

    if (index < 0 || _probeBatch->numProbes() <= index)
        return;

    const auto& _position = *reinterpret_cast<Vector3f*>(&position);

    _probeBatch->updateProbePosition(index, _position);
}

void IPLCALL iplProbeBatchUpdateEndpoint(IPLProbeBatch probeBatch, IPLBakedDataIdentifier identifier, IPLSphere endpointInfluence)
{
    if (!probeBatch)
        return;

    auto _probeBatch = reinterpret_cast<api::CProbeBatch*>(probeBatch)->mHandle.get();
    if (!_probeBatch)
        return;

    const auto& _identifier = *reinterpret_cast<BakedDataIdentifier*>(&identifier);
    const auto& _endpointInfluence = *reinterpret_cast<Sphere*>(&endpointInfluence);

    _probeBatch->updateEndpoint(_identifier, _endpointInfluence);
}

void IPLCALL iplProbeBatchGetInfluencingProbes(IPLProbeBatch probeBatch, IPLVector3 point, IPLProbeNeighborhood probeNeighborhood)
{
    if (!probeBatch || !probeNeighborhood)
        return;

    auto _probeBatch = reinterpret_cast<api::CProbeBatch*>(probeBatch)->mHandle.get();
    auto _probeNeighborhood = derefHandle<ProbeNeighborhood>(probeNeighborhood);
    if (!_probeBatch || !_probeNeighborhood)
        return;

    const auto& _position = *reinterpret_cast<Vector3f*>(&point);

    _probeBatch->getInfluencingProbes(_position, *_probeNeighborhood);
}

void IPLCALL iplProbeBatchGetProbeArray(IPLProbeBatch probeBatch, IPLProbeArray probeArray)
{
    if (!probeBatch || !probeArray)
        return;

    auto _probeBatch = reinterpret_cast<api::CProbeBatch*>(probeBatch)->mHandle.get();
    auto _probeArray = reinterpret_cast<api::CProbeArray*>(probeArray)->mHandle.get();
    if (!_probeBatch || !_probeArray)
        return;

    _probeBatch->toProbeArray(*_probeArray);
}

void IPLCALL iplSimulatorRunPathingPerSource(IPLSimulator simulator, IPLSource source)
{
    if (!simulator || !source)
        return;

    auto _simulator = reinterpret_cast<api::CSimulator*>(simulator)->mHandle.get();
    auto _source = reinterpret_cast<api::CSource*>(source)->mHandle.get();
    if (!_simulator || !_source)
        return;

    _simulator->simulatePathing(*_source);
}

void IPLCALL iplSimulatorRunPathingPerSourceForNeighborhood(IPLSimulator simulator, IPLSource source, IPLProbeNeighborhood listenerProbeNeighborhood)
{
    if (!simulator || !source || !listenerProbeNeighborhood)
        return;

    auto _simulator = reinterpret_cast<api::CSimulator*>(simulator)->mHandle.get();
    auto _source = reinterpret_cast<api::CSource*>(source)->mHandle.get();
    auto _listenerProbeNeighborhood = derefHandle<ProbeNeighborhood>(listenerProbeNeighborhood);
    if (!_simulator || !_source || !_listenerProbeNeighborhood)
        return;

    _simulator->simulatePathing(*_source, *_listenerProbeNeighborhood);
}

void IPLCALL iplSourceGetOutputsAux(IPLSource source, IPLSimulationFlags flags, IPLSimulationOutputsAux* outputs)
{
    if (!source || !outputs)
        return;

    auto _source = reinterpret_cast<api::CSource*>(source)->mHandle.get();
    if (!_source)
        return;

    if (flags & IPL_SIMULATIONFLAGS_PATHING)
    {
        outputs->pathingAvgDirection = *reinterpret_cast<IPLVector3*>(&_source->pathingOutputs.direction);
        outputs->pathingDistanceRatio = _source->pathingOutputs.distanceRatio;
    }
}


// --------------------------------------------------------------------------------------------------------------------
// Path Simulator
// --------------------------------------------------------------------------------------------------------------------

DEFINE_OPAQUE_HANDLE(IPLPathSimulator, PathSimulator);

IPLerror IPLCALL iplPathSimulatorCreate(IPLContext context, IPLPathSimulatorSettings* settings, IPLPathSimulator* simulator)
{
    if (!context || !settings || !simulator)
        return IPL_STATUS_FAILURE;

    auto _probeBatch = reinterpret_cast<api::CProbeBatch*>(settings->probeBatch)->mHandle.get();
    auto _asymmetricVisRange = (settings->asymmetricVisRange == IPL_TRUE);
    auto _down = *reinterpret_cast<Vector3f*>(&settings->down);
    if (!_probeBatch)
        return IPL_STATUS_FAILURE;

    auto _context = reinterpret_cast<api::CContext*>(context)->mHandle.get();
    if (!_context)
        return IPL_STATUS_FAILURE;

    try
    {
        *simulator = createHandle(_context, ipl::make_shared<PathSimulator>(*_probeBatch, settings->numSamples,
            _asymmetricVisRange, _down));
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLPathSimulator IPLCALL iplPathSimulatorRetain(IPLPathSimulator simulator)
{
    return retainHandle<PathSimulator>(simulator);
}

void IPLCALL iplPathSimulatorRelease(IPLPathSimulator* simulator)
{
    releaseHandle<PathSimulator>(*simulator);
}

void IPLCALL iplPathSimulatorSimulate(IPLPathSimulator simulator, IPLScene scene, IPLPathSimulatorParams* inputs, IPLPathEffectParams* outputs)
{
    if (!simulator || !scene || !inputs || !outputs)
        return;

    auto _simulator = derefHandle<PathSimulator>(simulator);
    auto _scene = reinterpret_cast<api::CScene*>(scene)->mHandle.get();
    auto _probeBatch = reinterpret_cast<api::CProbeBatch*>(inputs->probeBatch)->mHandle.get();
    auto _sourceProbes = derefHandle<ProbeNeighborhood>(inputs->sourceProbes);
    auto _listenerProbes = derefHandle<ProbeNeighborhood>(inputs->listenerProbes);
    if (!_simulator || !_scene || !_probeBatch || !_sourceProbes || !_listenerProbes)
        return;

    auto _source = *reinterpret_cast<Vector3f*>(&inputs->source);
    auto _listener = *reinterpret_cast<Vector3f*>(&inputs->listener);
    auto _enableValidation = (inputs->enableValidation == IPL_TRUE);
    auto _findAlternatePaths = (inputs->findAlternatePaths == IPL_TRUE);
    auto _simplifyPaths = (inputs->simplifyPaths == IPL_TRUE);
    auto _realTimeVis = (inputs->realTimeVis == IPL_TRUE);

    _simulator->findPaths(_source, _listener, *_scene, *_probeBatch, *_sourceProbes, *_listenerProbes, inputs->radius,
        inputs->threshold, inputs->visRange, inputs->order, _enableValidation, _findAlternatePaths,
        _simplifyPaths, _realTimeVis, outputs->eqCoeffs, outputs->shCoeffs);
}
