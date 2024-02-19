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

#include "directivity.h"
#include "energy_field.h"
#include "job_graph.h"
#include "sampling.h"
#include "scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

// Encapsulates the state required to simulate reflections. The simulation is performed using Monte Carlo path tracing.
// The state required to perform the simulation includes various arrays storing intermediate values calculated for
// each ray.
class IReflectionSimulator
{
public:
    virtual ~IReflectionSimulator()
    {}

    // Simulates reflections from multiple sources to a single receiver, storing the results in a single RGBA image.
    // This is useful for debugging and visualization.
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
                          JobGraph& jobGraph) = 0;

    // Simulates reflections from multiple sources to a multiple receivers, storing the results in an EnergyField
    // for each source.
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
                          JobGraph& jobGraph) = 0;

    // Simulates reflections from the origin, accumulating any rays that escape the scene. Used to test for
    // ray leakage.
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
                          vector<Ray>& escapedRays) = 0;

    static const float kHitSurfaceOffset;
    static const float kSpecularExponent;
    static const float kSourceRadius;
    static const float kListenerRadius;
};


// --------------------------------------------------------------------------------------------------------------------
// ReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

class ReflectionSimulator : public IReflectionSimulator
{
public:
    ReflectionSimulator(int maxNumRays,
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

    struct ThreadState
    {
        RandomNumberGenerator rng;
        Array<unique_ptr<EnergyField>> energyFields;
    };

    int mMaxNumRays;
    int mNumDiffuseSamples;
    float mMaxDuration;
    int mMaxOrder;
    int mMaxNumSources;
    int mNumThreads;

    int mNumSources;
    const CoordinateSpace3f* mSources;
    const CoordinateSpace3f* mListener;
    const Directivity* mDirectivities;
    int mNumRays;
    int mNumBounces;
    float mDuration;
    int mOrder;
    float mIrradianceMinDistance;

    Array<Vector3f> mListenerSamples;
    Array<Vector3f> mDiffuseSamples;
    Array<float, 2> mListenerCoeffs;
    std::atomic<int> mNumJobsRemaining;
    Array<ThreadState> mThreadState;

    void simulateJob(const IScene& scene,
                     Array<float, 2>& image,
                     int start,
                     int end,
                     int threadId);

    void simulateJob(const IScene& scene,
                     int start,
                     int end,
                     int threadId,
                     std::atomic<bool>& cancel);

    void finalizeJob(EnergyField* const* energyFields,
                     std::atomic<bool>& cancel);

    bool trace(const IScene& scene,
               const Ray& ray,
               int bounce,
               float accumDistance,
               Hit& hit,
               Vector3f& hitPoint);

    bool shade(const IScene& scene,
               const Ray& ray,
               int bounce,
               int sourceIndex,
               const Hit& hit,
               const Vector3f& hitPoint,
               const float* accumEnergy,
               float accumDistance,
               float scalar,
               float* energy,
               float& delay);

    void bounce(const IScene& scene,
                int bounce,
                const Hit& hit,
                const Vector3f& hitPoint,
                int threadId,
                Ray& ray,
                float* accumEnergy,
                float& accumDistance);
};


// --------------------------------------------------------------------------------------------------------------------
// BatchedReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

class BatchedReflectionSimulator : public IReflectionSimulator
{
public:
    BatchedReflectionSimulator(int maxNumRays,
                               int numDiffuseSamples,
                               float maxDuration,
                               int maxOrder,
                               int maxNumSources,
                               int numThreads,
                               int rayBatchSize);

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
    struct ThreadState
    {
        Array<Ray> rays;
        Array<float> minDistances;
        Array<float> maxDistances;
        Array<Hit> hits;
        Array<Ray> shadowRays;
        Array<float> shadowRayMinDistances;
        Array<float> shadowRayMaxDistances;
        Array<bool> occluded;
        Array<bool> escaped;
        Array<Vector3f> hitPoints;
        Array<float, 2> energy;
        Array<float> delay;
        Array<float, 2> accumEnergy;
        Array<float> accumDistance;
        RandomNumberGenerator rng;
        Array<unique_ptr<EnergyField>> energyFields;
    };

    int mMaxNumRays;
    int mNumDiffuseSamples;
    float mMaxDuration;
    int mMaxOrder;
    int mMaxNumSources;
    int mNumThreads;
    int mRayBatchSize;

    int mNumSources;
    const CoordinateSpace3f* mSources;
    const CoordinateSpace3f* mListener;
    const Directivity* mDirectivities;
    int mNumRays;
    int mNumBounces;
    float mDuration;
    int mOrder;
    float mIrradianceMinDistance;

    Array<Vector3f> mListenerSamples;
    Array<Vector3f> mDiffuseSamples;
    Array<float, 2> mListenerCoeffs;
    std::atomic<int> mNumJobsRemaining;
    Array<ThreadState> mThreadState;

    void simulateJob(const IScene& scene,
                     Array<float, 2>& image,
                     int start,
                     int end,
                     int threadId);

    void simulateJob(const IScene& scene,
                     int start,
                     int end,
                     int threadId,
                     std::atomic<bool>& cancel);

    void finalizeJob(EnergyField* const* energyFields,
                     std::atomic<bool>& cancel);

    void reset(int threadId);

    bool trace(const IScene& scene,
               int bounce,
               int start,
               int end,
               int threadId);

    void shade(const IScene& scene,
               int bounce,
               int sourceIndex,
               float scalar,
               int start,
               int end,
               int threadId);

    void bounce(const IScene& scene,
                int bounce,
                int start,
                int end,
                int threadId);

    bool escaped(const IScene& scene,
                 int bounce,
                 const Ray& ray,
                 const Hit& hit,
                 float accumDistance);
};

}
