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

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#include "embree_scene.h"
#include "embree_static_mesh.h"
#include "reflection_simulator.h"

#include "embree_reflection_simulator.ispc.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

class EmbreeReflectionSimulator : public IReflectionSimulator
{
public:
    EmbreeReflectionSimulator(int maxNumRays,
                              int numDiffuseSamples,
                              float maxDuration,
                              int maxOrder,
                              int maxNumSources,
                              int numThreads);

    virtual void simulate(const IScene& scene,
                          int numSources,
                          const CoordinateSpace3f* sources,
                          int numListeners,
                          const CoordinateSpace3f* listeners,
                          const Directivity* directivities,
                          int numRays,
                          int numBounces,
                          float duration,
                          int order,
                          float irradianceMinDistance,
                          Array<float, 2>& image,
                          JobGraph& jobGraph) override;

    virtual void simulate(const IScene& scene,
                          int numSources,
                          const CoordinateSpace3f* sources,
                          int numListeners,
                          const CoordinateSpace3f* listeners,
                          const Directivity* directivities,
                          int numRays,
                          int numBounces,
                          float duration,
                          int order,
                          float irradianceMinDistance,
                          EnergyField* const* energyFields,
                          JobGraph& jobGraph) override;

    virtual void simulate(const IScene& scene,
                          int numSources,
                          const CoordinateSpace3f* sources,
                          int numListeners,
                          const CoordinateSpace3f* listeners,
                          const Directivity* directivities,
                          int numRays,
                          int numBounces,
                          float duration,
                          int order,
                          float irradianceMinDistance,
                          vector<Ray>& escapedRays) override;

private:
    static const int kRayBatchSize;
    static const int kBlockSize;

    struct ThreadState
    {
        Array<unique_ptr<EnergyField>> energyFields;
    };

    int mMaxNumRays;
    int mNumDiffuseSamples;
    float mMaxDuration;
    int mMaxOrder;
    int mMaxNumSources;
    int mNumThreads;
    int mNumSources;
    Array<ispc::CoordinateSpace> mSources;
    Array<ispc::CoordinateSpace> mListener;
    Array<ispc::Directivity> mDirectivities;
    Array<ispc::EnergyField> mEnergyFields;
    int mNumRays;
    int mNumBounces;
    float mDuration;
    int mOrder;
    float mIrradianceMinDistance;
    Array<float, 2> mListenerSamples;
    Array<float, 2> mDiffuseSamples;
    Array<float, 2> mListenerCoeffs;
    std::atomic<int> mNumJobsRemaining;
    Array<ThreadState> mThreadState;
    ispc::EmbreeScene mScene;
    ispc::EmbreeReflectionSimulator mReflectionSimulator;

    static ispc::CoordinateSpace ispcCoordinateSpace(const CoordinateSpace3f& in);

    static ispc::Directivity ispcDirectivity(const Directivity& in);

    static ispc::Material ispcMaterial(const Material& in);

    static ispc::EnergyField ispcEnergyField(EnergyField& in);

    static ispc::EmbreeScene ispcEmbreeScene(const EmbreeScene& in);

    static ispc::EmbreeReflectionSimulator ispcEmbreeReflectionSimulator(const EmbreeReflectionSimulator& in);
};

}

#endif