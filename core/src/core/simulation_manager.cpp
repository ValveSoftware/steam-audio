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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SimulationManager
// --------------------------------------------------------------------------------------------------------------------

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
}

void SimulationManager::addProbeBatch(shared_ptr<ProbeBatch> probeBatch)
{
    mProbeManager->addProbeBatch(probeBatch);

    if (mEnablePathing)
    {
        mPathSimulators[probeBatch.get()] = make_unique<PathSimulator>(*probeBatch, mNumVisSamples, mAsymmetricVisRange, mDown);
    }
}

void SimulationManager::removeProbeBatch(shared_ptr<ProbeBatch> probeBatch)
{
    mProbeManager->removeProbeBatch(probeBatch);

    if (mEnablePathing)
    {
        mPathSimulators.erase(probeBatch.get());
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
    mDirectSimulator->simulate(mScene.get(), source.directInputs.flags, source.directInputs.source, mSharedData->direct.listener,
                               source.directInputs.distanceAttenuationModel, source.directInputs.airAbsorptionModel,
                               source.directInputs.directivity, source.directInputs.occlusionType, source.directInputs.occlusionRadius,
                               source.directInputs.numOcclusionSamples, source.directInputs.numTransmissionRays, source.directOutputs.directPath);
}

void SimulationManager::simulateIndirect()
{
    auto numChannels = SphericalHarmonics::numCoeffsForOrder(mSharedData->reflection.order);
    auto numSamples = static_cast<int>(ceilf(mSharedData->reflection.duration * mSamplingRate));

    mRealTimeSources.clear();
    mBakedSources.clear();
    mBakedDataIdentifiers.clear();
    mRealTimeDirectivities.clear();
    mRealTimeEnergyFields.clear();
    mAccumEnergyFields.clear();
    mBakedEnergyFields.clear();
    mBakedReverbs.clear();
    mEnergyFieldsForReconstruction.clear();
    mEnergyFieldsForCPUReconstruction.clear();
    mNumFramesAccumulated.clear();
    mDistanceAttenuationCorrectionCurves.clear();
    mAirAbsorptionModels.clear();
    mImpulseResponses.clear();

    auto listenerChanged = hasListenerChanged();
    auto sceneChanged = hasSceneChanged();

    for (auto& source : mSourceData[0])
    {
        auto sourceChanged = source->hasSourceChanged();

        if (source->reflectionInputs.enabled)
        {
            if (source->reflectionInputs.baked &&
                source->reflectionInputs.bakedDataIdentifier.type == BakedDataType::Reflections)
            {
                mBakedSources.push_back(source->reflectionInputs.source);
                mBakedReverbs.push_back(&source->reflectionOutputs.reverb);
                mBakedDataIdentifiers.push_back(source->reflectionInputs.bakedDataIdentifier);
                mBakedEnergyFields.push_back(source->reflectionState.accumEnergyField.get());
            }
            else if (!source->reflectionInputs.baked)
            {
                mRealTimeSources.push_back(source->reflectionInputs.source);
                mRealTimeDirectivities.push_back(source->reflectionInputs.directivity);

                // todo: gpu accumulation
                if (listenerChanged || sourceChanged || sceneChanged)
                {
                    mRealTimeEnergyFields.push_back(source->reflectionState.accumEnergyField.get());
                    source->reflectionState.numFramesAccumulated = 0;
                }
                else
                {
                    mRealTimeEnergyFields.push_back(source->reflectionState.energyField.get());
                }

                mAccumEnergyFields.push_back(source->reflectionState.accumEnergyField.get());

                mNumFramesAccumulated.push_back(source->reflectionState.numFramesAccumulated);
            }

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
    }

    if (mRealTimeSources.size() > 0)
    {
        mJobGraph.reset();

        mReflectionSimulator->simulate(*mScene, static_cast<int>(mRealTimeSources.size()), mRealTimeSources.data(), 1, &mSharedData->reflection.listener,
                                       mRealTimeDirectivities.data(), mSharedData->reflection.numRays, mSharedData->reflection.numBounces,
                                       mSharedData->reflection.duration, mSharedData->reflection.order, mSharedData->reflection.irradianceMinDistance,
                                       mRealTimeEnergyFields.data(), mJobGraph);

        mThreadPool->process(mJobGraph);

        for (auto i = 0u; i < mRealTimeSources.size(); ++i)
        {
            if (mRealTimeEnergyFields[i] != mAccumEnergyFields[i])
            {
                EnergyField::scale(*mAccumEnergyFields[i], static_cast<float>(mNumFramesAccumulated[i]), *mAccumEnergyFields[i]);
                EnergyField::add(*mRealTimeEnergyFields[i], *mAccumEnergyFields[i], *mAccumEnergyFields[i]);
                EnergyField::scale(*mAccumEnergyFields[i], 1.0f / (1.0f + mNumFramesAccumulated[i]), *mAccumEnergyFields[i]);
            }
        }

        for (auto& source : mSourceData[0])
        {
            if (source->reflectionInputs.enabled)
            {
                ++source->reflectionState.numFramesAccumulated;

                source->reflectionState.prevSource = source->reflectionInputs.source;
                source->reflectionState.prevDirectivity = source->reflectionInputs.directivity;
            }
        }

        mPrevListener = mSharedData->reflection.listener;
    }

    // TODO: Combine all probe occlusion checks into a single call to manage cases where batched
    // calls may be blocking.
    if (mBakedSources.size() > 0)
    {
        ProbeNeighborhood sourceProbes;
        ProbeNeighborhood listenerProbes;
        ProbeNeighborhood* probes = &listenerProbes;

        mProbeManager->getInfluencingProbes(mSharedData->reflection.listener.origin, listenerProbes);
        listenerProbes.checkOcclusion(*mScene, mSharedData->reflection.listener.origin);
        listenerProbes.calcWeights(mSharedData->reflection.listener.origin);

        for (auto i = 0u; i < mBakedSources.size(); ++i)
        {
            if (mBakedDataIdentifiers[i].variation == BakedDataVariation::StaticListener)
            {
                mProbeManager->getInfluencingProbes(mBakedSources[i].origin, sourceProbes);
                sourceProbes.checkOcclusion(*mScene, mBakedSources[i].origin);
                sourceProbes.calcWeights(mBakedSources[i].origin);

                probes = &sourceProbes;
            }

            if (mIndirectType != IndirectEffectType::Parametric)
            {
                BakedReflectionSimulator::lookupEnergyField(mBakedDataIdentifiers[i], *probes, *mBakedEnergyFields[i]);
            }

            if (mIndirectType == IndirectEffectType::Parametric || mIndirectType == IndirectEffectType::Hybrid)
            {
                BakedReflectionSimulator::lookupReverb(mBakedDataIdentifiers[i], *probes, *mBakedReverbs[i]);
            }
        }
    }

#if defined(IPL_USES_OPENCL)
    if (mSceneType == SceneType::RadeonRays && mIndirectType != IndirectEffectType::TrueAudioNext)
    {
        for (auto i = 0; i < mRealTimeSources.size(); ++i)
        {
            static_cast<OpenCLEnergyField&>(*mAccumEnergyFields[i]).copyDeviceToHost();
        }
    }
#endif

    for (auto& source : mSourceData[0])
    {
        if (source->reflectionInputs.enabled)
        {
            auto& energyField = *source->reflectionState.accumEnergyField;

            if (mIndirectType == IndirectEffectType::Parametric || mIndirectType == IndirectEffectType::Hybrid)
            {
                if (!source->reflectionInputs.baked)
                {
                    ReverbEstimator::estimate(energyField, source->reflectionInputs.airAbsorptionModel, source->reflectionOutputs.reverb);
                }

                if (source->reflectionInputs.reverbScale[0] != 1.0f ||
                    source->reflectionInputs.reverbScale[1] != 1.0f ||
                    source->reflectionInputs.reverbScale[2] != 1.0f)
                {
                    ReverbEstimator::applyReverbScale(source->reflectionInputs.reverbScale, energyField);

                    source->reflectionOutputs.reverb.reverbTimes[0] *= source->reflectionInputs.reverbScale[0];
                    source->reflectionOutputs.reverb.reverbTimes[1] *= source->reflectionInputs.reverbScale[1];
                    source->reflectionOutputs.reverb.reverbTimes[2] *= source->reflectionInputs.reverbScale[2];
                }
            }

            if (mIndirectType != IndirectEffectType::Parametric)
            {
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
                if (source->reflectionState.applyDistanceAttenuationCorrectionCurve)
                {
                    mDistanceAttenuationCorrectionCurves.push_back(source->reflectionState.distanceAttenuationCorrectionCurve.data());
                }
                else
                {
                    mDistanceAttenuationCorrectionCurves.push_back(nullptr);
                }
            }
        }
    }

    if (mIndirectType != IndirectEffectType::Parametric)
    {
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

    for (auto& source : mSourceData[0])
    {
        if (source->reflectionInputs.enabled)
        {
            auto energyField = source->reflectionState.accumEnergyField.get();
            auto impulseResponse = source->reflectionState.impulseResponse.get();

            if (mIndirectType == IndirectEffectType::Hybrid)
            {
                mHybridReverbEstimator->estimate(*energyField, source->reflectionOutputs.reverb, *impulseResponse,
                                                 source->reflectionInputs.transitionTime, source->reflectionInputs.overlapFraction,
                                                 mSharedData->reflection.order, source->reflectionOutputs.hybridEQ);

                source->reflectionOutputs.hybridDelay = static_cast<int>(ceilf((1.0f - source->reflectionInputs.overlapFraction) * source->reflectionInputs.transitionTime * mSamplingRate));
            }

            if (mIndirectType == IndirectEffectType::TrueAudioNext)
            {
#if defined(IPL_USES_TRUEAUDIONEXT)
                if (mSceneType != SceneType::RadeonRays || source->reflectionInputs.baked)
                {
                    static_cast<OpenCLImpulseResponse*>(impulseResponse)->copyHostToDevice();
                }

                if (source->reflectionOutputs.tanSlot >= 0)
                {
                    mTAN->setIR(source->reflectionOutputs.tanSlot, static_cast<OpenCLImpulseResponse*>(impulseResponse)->channelBuffers());
                }
#endif
            }
            else if (mIndirectType != IndirectEffectType::Parametric)
            {
                mPartitioner->partition(*impulseResponse, numChannels, numSamples, *source->reflectionOutputs.overlapSaveFIR.writeBuffer);
                source->reflectionOutputs.overlapSaveFIR.commitWriteBuffer();

                source->reflectionOutputs.numChannels = numChannels;
                source->reflectionOutputs.numSamples = numSamples;
            }
        }
    }

#if defined(IPL_USES_TRUEAUDIONEXT)
    if (mIndirectType == IndirectEffectType::TrueAudioNext)
    {
        mTAN->updateIRs();
    }
#endif

    resetSceneChanged();
}

void SimulationManager::simulatePathing()
{
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
            auto& simulator = *mPathSimulators[source->pathingInputs.probes.get()];

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
        auto& simulator = *mPathSimulators[source.pathingInputs.probes.get()];

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
    ProbeNeighborhood& sourceProbes = mTempSourcePathingProbes;

    if (sourceProbes.numProbes() != ProbeNeighborhood::kMaxProbesPerBatch)
        sourceProbes.resize(ProbeNeighborhood::kMaxProbesPerBatch);
    else
        sourceProbes.reset();

    if (source.pathingInputs.enabled)
    {
        auto probeBatch = source.pathingInputs.probes.get();
        auto& simulator = *mPathSimulators[source.pathingInputs.probes.get()];

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
