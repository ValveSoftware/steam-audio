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

#include "simulation_manager.h"

#include "baked_reflection_simulator.h"
#include "opencl_energy_field.h"
#include "opencl_impulse_response.h"
#include "reconstructor_factory.h"
#include "reflection_simulator_factory.h"
#include "sh.h"
#include "simulation_data.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SimulationManager
// --------------------------------------------------------------------------------------------------------------------

bool SimulationManager::sEnableProbeCachingForMissingProbes = false;

SimulationManager::SimulationManager(bool enableDirect,
                                     bool enableIndirect,
                                     bool enablePathing,
                                     SceneType sceneType,
                                     IndirectEffectType indirectType,
                                     int maxNumOcclusionSamples,
                                     int maxNumRays,
                                     int numDiffuseSamples,
                                     float maxDuration,
                                     int maxOrder,
                                     int maxNumSources,
                                     int maxNumListeners,
                                     int numThreads,
                                     int rayBatchSize,
                                     int numVisSamples,
                                     bool asymmetricVisRange,
                                     const Vector3f& down,
                                     int samplingRate,
                                     int frameSize,
                                     shared_ptr<OpenCLDevice> openCL,
                                     shared_ptr<RadeonRaysDevice> radeonRays,
                                     shared_ptr<TANDevice> tan)
    : mEnableDirect(enableDirect)
    , mEnableIndirect(enableIndirect)
    , mEnablePathing(enablePathing)
    , mSceneType(sceneType)
    , mIndirectType(indirectType)
    , mMaxNumOcclusionSamples(maxNumOcclusionSamples)
    , mMaxDuration(maxDuration)
    , mMaxOrder(maxOrder)
    , mNumVisSamples(numVisSamples)
    , mAsymmetricVisRange(asymmetricVisRange)
    , mDown(down)
    , mSamplingRate(samplingRate)
    , mFrameSize(frameSize)
    , mOpenCL(openCL)
    , mTAN(tan)
    , mSceneVersion(0)
{
    if (enableDirect)
    {
        mDirectSimulator = make_unique<DirectSimulator>(maxNumOcclusionSamples);
    }

    if (enablePathing || enableIndirect)
    {
        mProbeManager = make_unique<ProbeManager>();
    }

    if (enableIndirect)
    {
        mReflectionSimulator = ReflectionSimulatorFactory::create(sceneType, maxNumRays, numDiffuseSamples, maxDuration,
                                                                  maxOrder, maxNumSources, maxNumListeners, numThreads, rayBatchSize,
                                                                  radeonRays);

        if (indirectType != IndirectEffectType::Parametric)
        {
            mReconstructor = ReconstructorFactory::create(sceneType, indirectType, maxDuration, maxOrder,
                                                          samplingRate, radeonRays);

            if (sceneType == SceneType::RadeonRays && indirectType == IndirectEffectType::TrueAudioNext)
            {
                mCPUReconstructor = make_unique<Reconstructor>(maxDuration, maxOrder, samplingRate);
            }
        }

        if (indirectType == IndirectEffectType::Hybrid)
        {
            mHybridReverbEstimator = make_unique<HybridReverbEstimator>(maxDuration, samplingRate, frameSize);
        }

        if (indirectType == IndirectEffectType::Convolution || indirectType == IndirectEffectType::Hybrid)
        {
            mPartitioner = make_unique<OverlapSavePartitioner>(frameSize);
        }

        mThreadPool = make_unique<ThreadPool>(numThreads);
    }

    mSharedData = make_unique<SharedSimulationData>();
    mSharedData->reflection.numRays = maxNumRays;
    mSharedData->reflection.numBounces = 0;
    mSharedData->reflection.duration = maxDuration;
    mSharedData->reflection.order = maxOrder;
    mSharedData->reflection.irradianceMinDistance = 1.0f;
    mSharedData->reflection.reconstructionType = ReconstructionType::Linear;
    
    mProbeBatchesForLookup.reserve(16);
}

void SimulationManager::addProbeBatch(shared_ptr<ProbeBatch> probeBatch)
{
    if (mProbeManager)
    {
        mProbeManager->addProbeBatch(probeBatch);
    }

    if (mEnablePathing)
    {
        mPathSimulators[1][probeBatch.get()] = ipl::make_shared<PathSimulator>(*probeBatch, mNumVisSamples, mAsymmetricVisRange, mDown);
    }
}

void SimulationManager::removeProbeBatch(shared_ptr<ProbeBatch> probeBatch)
{
    if (mProbeManager)
    {
        mProbeManager->removeProbeBatch(probeBatch);
    }

    if (mEnablePathing)
    {
        mPathSimulators[1].erase(probeBatch.get());
    }
}

void SimulationManager::addSource(shared_ptr<SimulationData> source)
{
    mSourceData[1].push_back(source);
}

void SimulationManager::removeSource(shared_ptr<SimulationData> source)
{
    mSourceData[1].remove(source);
}

void SimulationManager::commit()
{
    if (mProbeManager)
    {
        mProbeManager->commit();
    }

    if (mEnablePathing)
    {
        mPathSimulators[0] = mPathSimulators[1];
    }

    mSourceData[0] = mSourceData[1];
}

void SimulationManager::simulateDirect()
{
    for (auto& source : mSourceData[0])
    {
        simulateDirect(*source);
    }
}

void SimulationManager::simulateDirect(SimulationData& source)
{
    PROFILE_FUNCTION();

    mDirectSimulator->simulate(mScene.get(), source.directInputs.flags, source.directInputs.source, mSharedData->direct.listener,
                               source.directInputs.distanceAttenuationModel, source.directInputs.airAbsorptionModel,
                               source.directInputs.directivity, source.directInputs.occlusionType, source.directInputs.occlusionRadius,
                               source.directInputs.numOcclusionSamples, source.directInputs.numTransmissionRays, source.directOutputs.directPath);
}

void SimulationManager::simulateIndirect()
{
    PROFILE_FUNCTION();

    auto numChannels = SphericalHarmonics::numCoeffsForOrder(mSharedData->reflection.order);
    auto numSamples = static_cast<int>(ceilf(mSharedData->reflection.duration * mSamplingRate));

    for (auto& source : mSourceData[0])
    {
        source->reflectionState.validSimulationData = true;
    }

    simulateRealTimeReflections();
    lookupBakedReflections();

    if (mSceneType == SceneType::RadeonRays && mIndirectType != IndirectEffectType::TrueAudioNext)
    {
        copyEnergyFieldsFromDeviceToHost();
    }

    if (mIndirectType != IndirectEffectType::Parametric)
    {
        generateDistanceCorrectionCurves(numSamples);
        reconstructImpulseResponses();
    }

    if (mIndirectType == IndirectEffectType::Parametric || mIndirectType == IndirectEffectType::Hybrid)
    {
        estimateReverb();
    }

    if (mIndirectType == IndirectEffectType::Hybrid)
    {
        estimateHybridReverb();
    }

    if (mSceneType != SceneType::RadeonRays && mIndirectType == IndirectEffectType::TrueAudioNext)
    {
        copyImpulseResponsesFromHostToDevice();
    }

    partitionImpulseResponses(numChannels, numSamples);

    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (!source->reflectionState.validSimulationData)
            continue;

        if (source->reflectionState.impulseResponseUpdated)
            continue;

        if (mIndirectType == IndirectEffectType::Convolution || mIndirectType == IndirectEffectType::Hybrid)
        {
            memcpy(source->reflectionState.impulseResponseCopy->data(),
                   source->reflectionState.impulseResponse->data(),
                   source->reflectionState.impulseResponse->numChannels() * source->reflectionState.impulseResponse->numSamples() * sizeof(float));
        }

        source->reflectionState.impulseResponseUpdated = true;
    }
}

void SimulationManager::simulateRealTimeReflections()
{
    PROFILE_FUNCTION();

    mRealTimeSources.clear();
    mRealTimeDirectivities.clear();
    mRealTimeEnergyFields.clear();

    auto listenerChanged = hasListenerChanged();
    auto sceneChanged = hasSceneChanged();

    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (source->reflectionInputs.baked)
            continue;

        mRealTimeSources.push_back(source->reflectionInputs.source);
        mRealTimeDirectivities.push_back(source->reflectionInputs.directivity);

        auto sourceChanged = source->hasSourceChanged();

        if (listenerChanged || sourceChanged || sceneChanged)
        {
            mRealTimeEnergyFields.push_back(source->reflectionState.accumEnergyField.get());
            source->reflectionState.numFramesAccumulated = 0;
        }
        else
        {
            mRealTimeEnergyFields.push_back(source->reflectionState.energyField.get());
        }
    }

    if (mRealTimeSources.empty())
        return;

    mJobGraph.reset();

    mReflectionSimulator->simulate(*mScene, static_cast<int>(mRealTimeSources.size()), mRealTimeSources.data(), 1, &mSharedData->reflection.listener,
                                   mRealTimeDirectivities.data(), mSharedData->reflection.numRays, mSharedData->reflection.numBounces,
                                   mSharedData->reflection.duration, mSharedData->reflection.order, mSharedData->reflection.irradianceMinDistance,
                                   mRealTimeEnergyFields.data(), mJobGraph);

    mThreadPool->process(mJobGraph);

    accumulateEnergyFields();
}

void SimulationManager::accumulateEnergyFields()
{
    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (source->reflectionInputs.baked)
            continue;

        if (source->reflectionState.numFramesAccumulated > 0)
        {
            EnergyField::scale(*source->reflectionState.accumEnergyField, static_cast<float>(source->reflectionState.numFramesAccumulated), *source->reflectionState.accumEnergyField);
            EnergyField::add(*source->reflectionState.energyField, *source->reflectionState.accumEnergyField, *source->reflectionState.accumEnergyField);
            EnergyField::scale(*source->reflectionState.accumEnergyField, 1.0f / (1.0f + source->reflectionState.numFramesAccumulated), *source->reflectionState.accumEnergyField);
        }

        ++source->reflectionState.numFramesAccumulated;

        source->reflectionState.prevSource = source->reflectionInputs.source;
        source->reflectionState.prevDirectivity = source->reflectionInputs.directivity;
    }

    mPrevListener = mSharedData->reflection.listener;

    resetSceneChanged();
}

void SimulationManager::lookupBakedReflections()
{
    PROFILE_FUNCTION();

    ProbeNeighborhood sourceProbes;
    ProbeNeighborhood listenerProbes;
    ProbeNeighborhood* probes = &listenerProbes;

    mProbeManager->getInfluencingProbes(mSharedData->reflection.listener.origin, listenerProbes);
    listenerProbes.checkOcclusion(*mScene, mSharedData->reflection.listener.origin);
    listenerProbes.calcWeights(mSharedData->reflection.listener.origin);

    for (auto& source : mSourceData[0])
    {
        PROFILE_ZONE("lookupBakedReflections::source");

        if (!source->reflectionInputs.enabled)
            continue;

        if (!source->reflectionInputs.baked || source->reflectionInputs.bakedDataIdentifier.type != BakedDataType::Reflections)
            continue;

        if (source->reflectionInputs.bakedDataIdentifier.variation == BakedDataVariation::StaticListener)
        {
            mProbeManager->getInfluencingProbes(source->reflectionInputs.source.origin, sourceProbes);
            sourceProbes.checkOcclusion(*mScene, source->reflectionInputs.source.origin);
            sourceProbes.calcWeights(source->reflectionInputs.source.origin);

            probes = &sourceProbes;
        }

        source->reflectionState.validSimulationData = sEnableProbeCachingForMissingProbes ? probes->hasValidProbes() : true;
        if (!source->reflectionState.validSimulationData)
            continue;

        BakedReflectionSimulator::findUniqueProbeBatches(*probes, mProbeBatchesForLookup);

        if (mIndirectType != IndirectEffectType::Parametric)
        {
            BakedReflectionSimulator::lookupEnergyField(source->reflectionInputs.bakedDataIdentifier, *probes, mProbeBatchesForLookup, *source->reflectionState.accumEnergyField);
        }

        if (mIndirectType == IndirectEffectType::Parametric || mIndirectType == IndirectEffectType::Hybrid)
        {
            BakedReflectionSimulator::lookupReverb(source->reflectionInputs.bakedDataIdentifier, *probes, mProbeBatchesForLookup, source->reflectionOutputs.reverb);
        }
    }
}

void SimulationManager::copyEnergyFieldsFromDeviceToHost()
{
#if defined(IPL_USES_OPENCL)
    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (source->reflectionInputs.baked)
            continue;

        static_cast<OpenCLEnergyField&>(*source->reflectionState.accumEnergyField).copyDeviceToHost();
    }
#endif
}

void SimulationManager::generateDistanceCorrectionCurves(int numSamples)
{
    PROFILE_FUNCTION();

    mDistanceAttenuationCorrectionCurves.clear();

    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (source->reflectionState.prevDistanceAttenuationModel != source->reflectionInputs.distanceAttenuationModel ||
            source->reflectionInputs.distanceAttenuationModel.dirty)
        {
            DistanceAttenuationModel::generateCorrectionCurve(DistanceAttenuationModel{},
                                                              source->reflectionInputs.distanceAttenuationModel,
                                                              mSamplingRate, numSamples,
                                                              source->reflectionState.distanceAttenuationCorrectionCurve.data());

            source->reflectionInputs.distanceAttenuationModel.dirty = false;
            source->reflectionState.prevDistanceAttenuationModel = source->reflectionInputs.distanceAttenuationModel;

            // From here on out, we will always apply a distance attenuation correction curve for this source.
            source->reflectionState.applyDistanceAttenuationCorrectionCurve = true;
        }

        // Now we build up the array of pointers to distance attenuation correction curves. If we have ever generated
        // a correction curve for this source, we will apply it during reconstruction.
        if (source->reflectionState.applyDistanceAttenuationCorrectionCurve && source->reflectionState.validSimulationData)
        {
            mDistanceAttenuationCorrectionCurves.push_back(source->reflectionState.distanceAttenuationCorrectionCurve.data());
        }
        else
        {
            mDistanceAttenuationCorrectionCurves.push_back(nullptr);
        }
    }
}

void SimulationManager::reconstructImpulseResponses()
{
    PROFILE_FUNCTION();

    mEnergyFieldsForReconstruction.clear();
    mEnergyFieldsForCPUReconstruction.clear();
    mAirAbsorptionModels.clear();
    mImpulseResponses.clear();

    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (mSceneType == SceneType::RadeonRays &&
            mIndirectType == IndirectEffectType::TrueAudioNext &&
            source->reflectionInputs.baked)
        {
            mEnergyFieldsForCPUReconstruction.push_back(source->reflectionState.accumEnergyField.get());
        }
        else
        {
            mEnergyFieldsForReconstruction.push_back(source->reflectionState.accumEnergyField.get());
        }

        mAirAbsorptionModels.push_back(source->reflectionInputs.airAbsorptionModel);
        mImpulseResponses.push_back(source->reflectionState.impulseResponse.get());
    }

    if (mEnergyFieldsForReconstruction.empty() && mEnergyFieldsForCPUReconstruction.empty())
        return;

    if (mSceneType == SceneType::RadeonRays &&
        mIndirectType == IndirectEffectType::TrueAudioNext &&
        mEnergyFieldsForCPUReconstruction.size() > 0)
    {
        mCPUReconstructor->reconstruct(static_cast<int>(mImpulseResponses.size()), mEnergyFieldsForCPUReconstruction.data(),
                                       mDistanceAttenuationCorrectionCurves.data(), mAirAbsorptionModels.data(),
                                       mImpulseResponses.data(), mSharedData->reflection.reconstructionType,
                                       mSharedData->reflection.duration, mSharedData->reflection.order);
    }

    if (mEnergyFieldsForReconstruction.size() > 0)
    {
        mReconstructor->reconstruct(static_cast<int>(mImpulseResponses.size()), mEnergyFieldsForReconstruction.data(),
                                    mDistanceAttenuationCorrectionCurves.data(), mAirAbsorptionModels.data(),
                                    mImpulseResponses.data(), mSharedData->reflection.reconstructionType,
                                    mSharedData->reflection.duration, mSharedData->reflection.order);
    }
}

void SimulationManager::estimateReverb()
{
    PROFILE_FUNCTION();

    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (!source->reflectionInputs.baked)
        {
            ReverbEstimator::estimate(*source->reflectionState.accumEnergyField, source->reflectionInputs.airAbsorptionModel, source->reflectionOutputs.reverb);
        }

        if (source->reflectionState.validSimulationData && (source->reflectionInputs.reverbScale[0] != 1.0f ||
                                                            source->reflectionInputs.reverbScale[1] != 1.0f ||
                                                            source->reflectionInputs.reverbScale[2] != 1.0f))
        {
            ReverbEstimator::applyReverbScale(source->reflectionInputs.reverbScale, *source->reflectionState.accumEnergyField);

            source->reflectionOutputs.reverb.reverbTimes[0] *= source->reflectionInputs.reverbScale[0];
            source->reflectionOutputs.reverb.reverbTimes[1] *= source->reflectionInputs.reverbScale[1];
            source->reflectionOutputs.reverb.reverbTimes[2] *= source->reflectionInputs.reverbScale[2];
        }
    }
}

void SimulationManager::estimateHybridReverb()
{
    PROFILE_FUNCTION();

    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (!source->reflectionState.validSimulationData)
            continue;

        mHybridReverbEstimator->estimate(source->reflectionState.accumEnergyField.get(), source->reflectionOutputs.reverb, *source->reflectionState.impulseResponse,
                                         source->reflectionInputs.transitionTime, source->reflectionInputs.overlapFraction,
                                         mSharedData->reflection.order, source->reflectionOutputs.hybridEQ, source->reflectionOutputs.hybridDelay);
    }
}

void SimulationManager::copyImpulseResponsesFromHostToDevice()
{
#if defined(IPL_USES_OPENCL)
    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (!source->reflectionState.validSimulationData)
            continue;

        static_cast<OpenCLImpulseResponse*>(source->reflectionState.impulseResponse.get())->copyHostToDevice();
    }
#endif
}

void SimulationManager::partitionImpulseResponses(int numChannels, int numSamples)
{
    PROFILE_FUNCTION();

    for (auto& source : mSourceData[0])
    {
        if (!source->reflectionInputs.enabled)
            continue;

        if (!source->reflectionState.validSimulationData)
            continue;

        if (mIndirectType == IndirectEffectType::TrueAudioNext)
        {
#if defined(IPL_USES_TRUEAUDIONEXT)
            if (source->reflectionOutputs.tanSlot >= 0)
            {
                mTAN->setIR(source->reflectionOutputs.tanSlot, static_cast<OpenCLImpulseResponse*>(source->reflectionState.impulseResponse.get())->channelBuffers());
            }
#endif
        }
        else if (mIndirectType != IndirectEffectType::Parametric)
        {
            mPartitioner->partition(*source->reflectionState.impulseResponse, numChannels, numSamples, *source->reflectionOutputs.overlapSaveFIR.writeBuffer);

            source->reflectionOutputs.overlapSaveFIR.commitWriteBuffer();
            source->reflectionOutputs.numChannels = numChannels;
            source->reflectionOutputs.numSamples = numSamples;
        }
    }

#if defined(IPL_USES_TRUEAUDIONEXT)
    if (mIndirectType == IndirectEffectType::TrueAudioNext)
    {
        mTAN->updateIRs();
    }
#endif
}

void SimulationManager::simulatePathing()
{
    PROFILE_FUNCTION();

    ProbeNeighborhood sourceProbes;
    ProbeNeighborhood listenerProbes;
    ProbeBatch* prevListenerProbeBatch = nullptr;

    sourceProbes.resize(ProbeNeighborhood::kMaxProbesPerBatch);
    listenerProbes.resize(ProbeNeighborhood::kMaxProbesPerBatch);

    for (auto& source : mSourceData[0])
    {
        if (source->pathingInputs.enabled)
        {
            auto probeBatch = source->pathingInputs.probes.get();
            auto& simulator = *mPathSimulators[0][source->pathingInputs.probes.get()];

            sourceProbes.reset();
            probeBatch->getInfluencingProbes(source->pathingInputs.source.origin, sourceProbes);
            sourceProbes.checkOcclusion(*mScene, source->pathingInputs.source.origin);
            sourceProbes.calcWeights(source->pathingInputs.source.origin);

            if (prevListenerProbeBatch != probeBatch)
            {
                prevListenerProbeBatch = probeBatch;

                listenerProbes.reset();
                probeBatch->getInfluencingProbes(mSharedData->pathing.listener.origin, listenerProbes);
                listenerProbes.checkOcclusion(*mScene, mSharedData->pathing.listener.origin);
                listenerProbes.calcWeights(mSharedData->pathing.listener.origin);
            }

            simulator.findPaths(source->pathingInputs.source.origin, mSharedData->pathing.listener.origin, *mScene, *probeBatch, sourceProbes,
                                listenerProbes, source->pathingInputs.visRadius, source->pathingInputs.visThreshold, source->pathingInputs.visRange,
                                source->pathingInputs.order, source->pathingInputs.enableValidation, source->pathingInputs.findAlternatePaths,
                                source->pathingInputs.simplifyPaths, source->pathingInputs.realTimeVis,
                                source->pathingState.eq, source->pathingState.sh.data(), &source->pathingState.direction, &source->pathingState.distanceRatio, mSharedData->pathing.visCallback, mSharedData->pathing.userData);

            memcpy(source->pathingOutputs.eq, source->pathingState.eq, Bands::kNumBands * sizeof(float));
            memcpy(source->pathingOutputs.sh.data(), source->pathingState.sh.data(), source->pathingOutputs.sh.totalSize() * sizeof(float));
            source->pathingOutputs.direction = source->pathingState.direction;
            source->pathingOutputs.distanceRatio = source->pathingState.distanceRatio;
        }
    }
}

void SimulationManager::simulatePathing(SimulationData& source)
{
    PROFILE_FUNCTION();

    ProbeNeighborhood& sourceProbes = mTempSourcePathingProbes;
    ProbeNeighborhood& listenerProbes = mTempListenerPathingProbes;

    if (sourceProbes.numProbes() != ProbeNeighborhood::kMaxProbesPerBatch)
        sourceProbes.resize(ProbeNeighborhood::kMaxProbesPerBatch);
    else
        sourceProbes.reset();

    if (listenerProbes.numProbes() != ProbeNeighborhood::kMaxProbesPerBatch)
        listenerProbes.resize(ProbeNeighborhood::kMaxProbesPerBatch);
    else
        listenerProbes.reset();

    if (source.pathingInputs.enabled)
    {
        auto probeBatch = source.pathingInputs.probes.get();
        auto& simulator = *mPathSimulators[0][source.pathingInputs.probes.get()];

        probeBatch->getInfluencingProbes(source.pathingInputs.source.origin, sourceProbes);
        sourceProbes.checkOcclusion(*mScene, source.pathingInputs.source.origin);
        sourceProbes.calcWeights(source.pathingInputs.source.origin);

        probeBatch->getInfluencingProbes(mSharedData->pathing.listener.origin, listenerProbes);
        listenerProbes.checkOcclusion(*mScene, mSharedData->pathing.listener.origin);
        listenerProbes.calcWeights(mSharedData->pathing.listener.origin);

        simulator.findPaths(source.pathingInputs.source.origin, mSharedData->pathing.listener.origin, *mScene, *probeBatch, sourceProbes,
            listenerProbes, source.pathingInputs.visRadius, source.pathingInputs.visThreshold, source.pathingInputs.visRange,
            source.pathingInputs.order, source.pathingInputs.enableValidation, source.pathingInputs.findAlternatePaths,
            source.pathingInputs.simplifyPaths, source.pathingInputs.realTimeVis,
            source.pathingState.eq, source.pathingState.sh.data(), &source.pathingState.direction, &source.pathingState.distanceRatio, mSharedData->pathing.visCallback, mSharedData->pathing.userData);

        memcpy(source.pathingOutputs.eq, source.pathingState.eq, Bands::kNumBands * sizeof(float));
        memcpy(source.pathingOutputs.sh.data(), source.pathingState.sh.data(), source.pathingOutputs.sh.totalSize() * sizeof(float));
        source.pathingOutputs.direction = source.pathingState.direction;
        source.pathingOutputs.distanceRatio = source.pathingState.distanceRatio;
    }
}

void SimulationManager::simulatePathing(SimulationData& source, ProbeNeighborhood& listenerProbeNeighborhood)
{
    PROFILE_FUNCTION();

    ProbeNeighborhood& sourceProbes = mTempSourcePathingProbes;

    if (sourceProbes.numProbes() != ProbeNeighborhood::kMaxProbesPerBatch)
        sourceProbes.resize(ProbeNeighborhood::kMaxProbesPerBatch);
    else
        sourceProbes.reset();

    if (source.pathingInputs.enabled)
    {
        auto probeBatch = source.pathingInputs.probes.get();
        auto& simulator = *mPathSimulators[0][source.pathingInputs.probes.get()];

        probeBatch->getInfluencingProbes(source.pathingInputs.source.origin, sourceProbes);
        sourceProbes.checkOcclusion(*mScene, source.pathingInputs.source.origin);
        sourceProbes.calcWeights(source.pathingInputs.source.origin);

        simulator.findPaths(source.pathingInputs.source.origin, mSharedData->pathing.listener.origin, *mScene, *probeBatch, sourceProbes,
            listenerProbeNeighborhood, source.pathingInputs.visRadius, source.pathingInputs.visThreshold, source.pathingInputs.visRange,
            source.pathingInputs.order, source.pathingInputs.enableValidation, source.pathingInputs.findAlternatePaths,
            source.pathingInputs.simplifyPaths, source.pathingInputs.realTimeVis,
            source.pathingState.eq, source.pathingState.sh.data(), &source.pathingState.direction, &source.pathingState.distanceRatio, mSharedData->pathing.visCallback, mSharedData->pathing.userData);

        memcpy(source.pathingOutputs.eq, source.pathingState.eq, Bands::kNumBands * sizeof(float));
        memcpy(source.pathingOutputs.sh.data(), source.pathingState.sh.data(), source.pathingOutputs.sh.totalSize() * sizeof(float));
        source.pathingOutputs.direction = source.pathingState.direction;
        source.pathingOutputs.distanceRatio = source.pathingState.distanceRatio;
    }
}

void SimulationManager::simulatePathing(SimulationData& source, ProbeNeighborhood& sourceProbeNeighborhood, ProbeNeighborhood& listenerProbeNeighborhood)
{
    PROFILE_FUNCTION();

    if (source.pathingInputs.enabled)
    {
        auto probeBatch = source.pathingInputs.probes.get();
        auto& simulator = *mPathSimulators[0][source.pathingInputs.probes.get()];

        simulator.findPaths(source.pathingInputs.source.origin, mSharedData->pathing.listener.origin, *mScene, *probeBatch, sourceProbeNeighborhood,
            listenerProbeNeighborhood, source.pathingInputs.visRadius, source.pathingInputs.visThreshold, source.pathingInputs.visRange,
            source.pathingInputs.order, source.pathingInputs.enableValidation, source.pathingInputs.findAlternatePaths,
            source.pathingInputs.simplifyPaths, source.pathingInputs.realTimeVis,
            source.pathingState.eq, source.pathingState.sh.data(), &source.pathingState.direction, &source.pathingState.distanceRatio, mSharedData->pathing.visCallback, mSharedData->pathing.userData, true);

        memcpy(source.pathingOutputs.eq, source.pathingState.eq, Bands::kNumBands * sizeof(float));
        memcpy(source.pathingOutputs.sh.data(), source.pathingState.sh.data(), source.pathingOutputs.sh.totalSize() * sizeof(float));
        source.pathingOutputs.direction = source.pathingState.direction;
        source.pathingOutputs.distanceRatio = source.pathingState.distanceRatio;
    }
}

bool SimulationManager::hasListenerChanged() const
{
    auto changed = ((mSharedData->reflection.listener.origin - mPrevListener.origin).length() > 1e-4f);
    return changed;
}

bool SimulationManager::hasSceneChanged()
{
    // We don't currently have an implementation of energy field accumulation in OpenCL. So if we're using
    // Radeon Rays, always assume that the scene has changed, so accumulation never runs.
    if (mSceneType == SceneType::RadeonRays)
        return true;

    // We need to implement API callbacks to allow user-provided ray tracers to report whether the
    // scene has changed. For the time being, just assume that the scene has always changed, so
    // accumulation never runs.
    if (mSceneType == SceneType::Custom)
        return true;

    auto version = mScene->version();
    if (version != mSceneVersion)
        return true;

    return false;
}

void SimulationManager::resetSceneChanged()
{
    mSceneVersion = mScene->version();
}

}
