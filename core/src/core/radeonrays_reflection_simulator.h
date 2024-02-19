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

#if defined(IPL_USES_RADEONRAYS)

#include "sampling.h"
#include "opencl_energy_field.h"
#include "radeonrays_scene.h"
#include "reflection_simulator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

class RadeonRaysReflectionSimulator : public IReflectionSimulator
{
public:
    static const float kHistogramScale;

    RadeonRaysReflectionSimulator(int maxNumRays,
                                  int numDiffuseSamples,
                                  float maxDuration,
                                  int maxOrder,
                                  int maxNumSources,
                                  int maxNumListeners,
                                  shared_ptr<RadeonRaysDevice> radeonRays);

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
    shared_ptr<RadeonRaysDevice> mRadeonRays;

    int mMaxNumRays;
    int mNumDiffuseSamples;
    float mMaxDuration;
    int mMaxOrder;
    int mMaxNumSources;
    int mMaxNumListeners;

    RandomNumberGenerator mRNG;

    OpenCLBuffer mSources;
    OpenCLBuffer mListeners;
    OpenCLBuffer mDirectivities;

    OpenCLBuffer mListenerSamples;
    OpenCLBuffer mDiffuseSamples;
    OpenCLBuffer mListenerCoeffs;
    OpenCLBuffer mEnergy;
    OpenCLBuffer mAccumEnergy;
    OpenCLBuffer mImage;

    RadeonRaysBuffer mRays[2];
    int mCurrentRayBuffer;
    RadeonRaysBuffer mNumRays;
    RadeonRaysBuffer mHits;
    RadeonRaysBuffer mShadowRays;
    RadeonRaysBuffer mNumShadowRays;
    RadeonRaysBuffer mOccluded;

    OpenCLKernel mGenerateCameraRays;
    OpenCLKernel mGenerateListenerRays;
    OpenCLKernel mSphereOcclusion;
    OpenCLKernel mShadeAndBounce;
    OpenCLKernel mGatherImage;
    OpenCLKernel mGatherEnergyField;
    //OpenCLKernel mAccumEnergyFields;

    int mShadeLocalSize;

    void reset();

    void setSourcesAndListeners(int numSources,
                                const CoordinateSpace3f* sources,
                                int numListeners,
                                const CoordinateSpace3f* listeners,
                                const Directivity* directivities);

    void setNumRays(int numSources,
                    int numListeners,
                    int numRays);

    void generateCameraRays(int numRays);

    void generateListenerRays(int numListeners,
                              int numRays);

    void sphereOcclusion(int numSources,
                         const CoordinateSpace3f* sources,
                         int numListeners,
                         const CoordinateSpace3f* listeners,
                         int numRays);

    void shadeAndBounce(const RadeonRaysScene& scene,
                        int numSources,
                        const CoordinateSpace3f* sources,
                        int numListeners,
                        const CoordinateSpace3f* listeners,
                        const Directivity* directivities,
                        int numRays,
                        int numBounces,
                        float irradianceMinDistance,
                        float scalar);

    void gatherImage(int numSources,
                     int numRays);

    void gatherEnergyField(int index,
                           int numRays,
                           OpenCLEnergyField& energyField);
};

}

#endif
