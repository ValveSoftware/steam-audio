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

#include "direct_simulator.h"
#include "hybrid_reverb_estimator.h"
#include "indirect_effect.h"
#include "opencl_device.h"
#include "overlap_save_convolution_effect.h"
#include "path_simulator.h"
#include "probe_manager.h"
#include "reconstructor.h"
#include "reflection_simulator.h"
#include "scene_factory.h"
#include "tan_device.h"
#include "tan_convolution_effect.h"
#include "thread_pool.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SimulationManager
// --------------------------------------------------------------------------------------------------------------------

class SimulationData;

struct SharedDirectSimulationInputs
{
    CoordinateSpace3f listener;
};

struct SharedReflectionSimulationInputs
{
    CoordinateSpace3f listener;
    int numRays;
    int numBounces;
    float duration;
    int order;
    float irradianceMinDistance;
    ReconstructionType reconstructionType;
};

struct SharedPathingSimulationInputs
{
    CoordinateSpace3f listener;
    ValidationRayVisualizationCallback visCallback = nullptr;
    void* userData = nullptr;
};

struct SharedSimulationData
{
    SharedDirectSimulationInputs direct;
    SharedReflectionSimulationInputs reflection;
    SharedPathingSimulationInputs pathing;
};

class SimulationManager
{
public:
    static bool sEnableProbeCachingForMissingProbes;

    SimulationManager(bool enableDirect,
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
                      shared_ptr<TANDevice> tan);

    shared_ptr<IScene>& scene()
    {
        return mScene;
    }

    const shared_ptr<IScene>& scene() const
    {
        return mScene;
    }

    SceneType sceneType() const
    {
        return mSceneType;
    }

    IndirectEffectType indirectType() const
    {
        return mIndirectType;
    }

    int maxNumOcclusionSamples() const
    {
        return mMaxNumOcclusionSamples;
    }

    float maxDuration() const
    {
        return mMaxDuration;
    }

    int maxOrder() const
    {
        return mMaxOrder;
    }

    int samplingRate() const
    {
        return mSamplingRate;
    }

    int frameSize() const
    {
        return mFrameSize;
    }

    shared_ptr<OpenCLDevice> openCLDevice()
    {
        return mOpenCL;
    }

    shared_ptr<TANDevice> tanDevice()
    {
        return mTAN;
    }

    void setSharedDirectInputs(const SharedDirectSimulationInputs& sharedDirectInputs)
    {
        mSharedData->direct = sharedDirectInputs;
    }

    void setSharedReflectionInputs(const SharedReflectionSimulationInputs& sharedReflectionInputs)
    {
        mSharedData->reflection = sharedReflectionInputs;
    }

    void setSharedPathingInputs(const SharedPathingSimulationInputs& sharedPathingInputs)
    {
        mSharedData->pathing = sharedPathingInputs;
    }

    void addProbeBatch(shared_ptr<ProbeBatch> probeBatch);

    void removeProbeBatch(shared_ptr<ProbeBatch> probeBatch);

    void addSource(shared_ptr<SimulationData> source);

    void removeSource(shared_ptr<SimulationData> source);

    void commit();

    void simulateDirect();

    void simulateDirect(SimulationData& source);

    void simulateIndirect();

    void simulatePathing();

    void simulatePathing(SimulationData& source);

    void simulatePathing(SimulationData& source, ProbeNeighborhood& listenerProbeNeighborhood);

    void simulatePathing(SimulationData& source, ProbeNeighborhood& sourceProbeNeighborhood, ProbeNeighborhood& listenerProbeNeighborhood);

private:
    bool mEnableDirect;
    bool mEnableIndirect;
    bool mEnablePathing;
    SceneType mSceneType;
    IndirectEffectType mIndirectType;
    int mMaxNumOcclusionSamples;
    float mMaxDuration;
    int mMaxOrder;
    int mNumVisSamples;
    bool mAsymmetricVisRange;
    Vector3f mDown;
    int mSamplingRate;
    int mFrameSize;
    shared_ptr<IScene> mScene;
    unique_ptr<ProbeManager> mProbeManager;
    unique_ptr<DirectSimulator> mDirectSimulator;
    unique_ptr<IReflectionSimulator> mReflectionSimulator;
    unique_ptr<IReconstructor> mReconstructor;
    unique_ptr<IReconstructor> mCPUReconstructor;
    unique_ptr<HybridReverbEstimator> mHybridReverbEstimator;
    unique_ptr<OverlapSavePartitioner> mPartitioner;
    shared_ptr<OpenCLDevice> mOpenCL;
    shared_ptr<TANDevice> mTAN;
    map<const ProbeBatch*, shared_ptr<PathSimulator>> mPathSimulators[2];
    JobGraph mJobGraph;
    unique_ptr<ThreadPool> mThreadPool;
    unique_ptr<SharedSimulationData> mSharedData;
    CoordinateSpace3f mPrevListener;
    list<shared_ptr<SimulationData>> mSourceData[2];
    vector<CoordinateSpace3f> mRealTimeSources;
    vector<Directivity> mRealTimeDirectivities;
    vector<EnergyField*> mRealTimeEnergyFields;
    vector<EnergyField*> mAccumEnergyFields;
    vector<EnergyField*> mEnergyFieldsForReconstruction;
    vector<EnergyField*> mEnergyFieldsForCPUReconstruction;
    vector<const float*> mDistanceAttenuationCorrectionCurves;
    vector<AirAbsorptionModel> mAirAbsorptionModels;
    vector<ImpulseResponse*> mImpulseResponses;
    ProbeNeighborhood mTempSourcePathingProbes;
    ProbeNeighborhood mTempListenerPathingProbes;
    unordered_set<const ProbeBatch*> mProbeBatchesForLookup;

    // Version number of the scene when simulateIndirect() was last called.
    uint32_t mSceneVersion;

    bool hasListenerChanged() const;

    // Returns true if the scene has changed since the last call to simulateIndirect().
    bool hasSceneChanged();

    // Records that we have used the latest version of the scene.
    void resetSceneChanged();

    void simulateRealTimeReflections();
    void accumulateEnergyFields();
    void lookupBakedReflections();
    void copyEnergyFieldsFromDeviceToHost();
    void generateDistanceCorrectionCurves(int numSamples);
    void reconstructImpulseResponses();
    void estimateReverb();
    void estimateHybridReverb();
    void copyImpulseResponsesFromHostToDevice();
    void partitionImpulseResponses(int numChannels, int numSamples);
};

}
