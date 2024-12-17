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

#include "reflection_simulator.h"

#include "direct_simulator.h"
#include "propagation_medium.h"
#include "sh.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

const float IReflectionSimulator::kHitSurfaceOffset = 1e-2f;
const float IReflectionSimulator::kSpecularExponent = 1e2f;
const float IReflectionSimulator::kSourceRadius = 0.1f;
const float IReflectionSimulator::kListenerRadius = 0.1f;


// --------------------------------------------------------------------------------------------------------------------
// ReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

const int ReflectionSimulator::kRayBatchSize = 32;

ReflectionSimulator::ReflectionSimulator(int maxNumRays,
                                         int numDiffuseSamples,
                                         float maxDuration,
                                         int maxOrder,
                                         int maxNumSources,
                                         int numThreads)
    : mMaxNumRays(maxNumRays)
    , mNumDiffuseSamples(numDiffuseSamples)
    , mMaxDuration(maxDuration)
    , mMaxOrder(maxOrder)
    , mMaxNumSources(maxNumSources)
    , mNumThreads(numThreads)
    , mNumSources(maxNumSources)
    , mSources(nullptr)
    , mListener(nullptr)
    , mDirectivities(nullptr)
    , mNumRays(maxNumRays)
    , mNumBounces(0)
    , mDuration(maxDuration)
    , mOrder(maxOrder)
    , mIrradianceMinDistance(1.0f)
    , mListenerSamples(maxNumRays)
    , mDiffuseSamples(numDiffuseSamples)
    , mListenerCoeffs(maxNumRays, SphericalHarmonics::numCoeffsForOrder(maxOrder))
    , mThreadState(numThreads)
{
    Sampling::generateSphereSamples(maxNumRays, mListenerSamples.data());
    Sampling::generateHemisphereSamples(numDiffuseSamples, mDiffuseSamples.data());

    for (auto i = 0; i < maxNumRays; ++i)
    {
        for (auto l = 0, j = 0; l <= maxOrder; ++l)
        {
            for (auto m = -l; m <= l; ++m, ++j)
            {
                mListenerCoeffs[i][j] = SphericalHarmonics::evaluate(l, m, mListenerSamples[i]);
            }
        }
    }

    for (auto i = 0; i < numThreads; ++i)
    {
        mThreadState[i].energyFields.resize(maxNumSources);
        for (auto j = 0; j < maxNumSources; ++j)
        {
            mThreadState[i].energyFields[j] = make_unique<EnergyField>(maxDuration, maxOrder);
        }
    }
}

void ReflectionSimulator::simulate(const IScene& scene,
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
                                   JobGraph& jobGraph)
{
    PROFILE_FUNCTION();

    assert(numListeners == 1);

    // If we've been asked to simulate more sources than the max number we were initialized with, don't process the
    // extra ones.
    if (numSources > mMaxNumSources)
    {
        gLog().message(MessageSeverity::Warning,
            "Simulating reflections for %d sources, which is more than the max (%d). Some sources will be ignored.",
            numSources, mMaxNumSources);

        numSources = mMaxNumSources;
    }

    mNumSources = numSources;
    mSources = sources;
    mListener = &listeners[0];
    mDirectivities = directivities;
    mNumRays = numRays;
    mNumBounces = numBounces;
    mDuration = duration;
    mOrder = order;
    mIrradianceMinDistance = irradianceMinDistance;

    image.zero();

    for (auto i = 0; i < numRays; i += kRayBatchSize)
    {
        auto start = i;
        auto end = std::min(numRays, i + kRayBatchSize);

        jobGraph.addJob([this, &scene, &image, start, end](int threadId, std::atomic<bool>& cancel)
        {
            simulateJob(scene, image, start, end, threadId);
        });
    }
}

void ReflectionSimulator::simulate(const IScene& scene,
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
                                   JobGraph& jobGraph)
{
    PROFILE_FUNCTION();

    assert(numListeners == 1);

    // If we've been asked to simulate more sources than the max number we were initialized with, don't process the
    // extra ones.
    if (numSources > mMaxNumSources)
    {
        gLog().message(MessageSeverity::Warning,
            "Simulating reflections for %d sources, which is more than the max (%d). Some sources will be ignored.",
            numSources, mMaxNumSources);

        numSources = mMaxNumSources;
    }

    mNumSources = numSources;
    mSources = sources;
    mListener = &listeners[0];
    mDirectivities = directivities;
    mNumRays = numRays;
    mNumBounces = numBounces;
    mDuration = duration;
    mOrder = order;
    mIrradianceMinDistance = irradianceMinDistance;

    for (auto i = 0; i < numSources; ++i)
    {
        energyFields[i]->reset();
        for (auto j = 0; j < mNumThreads; ++j)
        {
            mThreadState[j].energyFields[i]->reset();
        }
    }

    mNumJobsRemaining = 0;

    for (auto i = 0; i < numRays; i += kRayBatchSize)
    {
        ++mNumJobsRemaining;

        auto start = i;
        auto end = std::min(numRays, i + kRayBatchSize);

        jobGraph.addJob([this, &scene, energyFields, start, end](int threadId, std::atomic<bool>& cancel)
        {
            simulateJob(scene, start, end, threadId, cancel);

            if (--mNumJobsRemaining == 0)
            {
                finalizeJob(energyFields, cancel);
            }
        });
    }
}

void ReflectionSimulator::simulate(const IScene& scene,
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
                                   vector<Ray>& escapedRays)
{
    PROFILE_FUNCTION();

    // If we've been asked to simulate more sources than the max number we were initialized with, don't process the
    // extra ones.
    if (numSources > mMaxNumSources)
    {
        gLog().message(MessageSeverity::Warning,
            "Simulating reflections for %d sources, which is more than the max (%d). Some sources will be ignored.",
            numSources, mMaxNumSources);

        numSources = mMaxNumSources;
    }

    mNumSources = numSources;
    mSources = sources;
    mListener = &listeners[0];
    mDirectivities = directivities;
    mNumRays = numRays;
    mNumBounces = numBounces;
    mDuration = duration;
    mOrder = order;
    mIrradianceMinDistance = irradianceMinDistance;

    for (auto i = 0; i < numRays; ++i)
    {
        Ray ray{ listeners[0].origin, mListenerSamples[i] };

        float accumEnergy[Bands::kNumBands] = { 1.0f, 1.0f, 1.0f };
        auto accumDistance = 0.0f;

        for (auto j = 0; j < numBounces; ++j)
        {
            Hit hit;
            Vector3f hitPoint;
            if (!trace(scene, ray, j, accumDistance, hit, hitPoint))
            {
                if (!hit.isValid())
                {
                    escapedRays.push_back(ray);
                }

                break;
            }

            auto testRay = Ray{ hitPoint, -hit.normal };
            auto testHit = scene.closestHit(testRay, 0.0f, std::numeric_limits<float>::infinity());
            if (testHit.objectIndex == hit.objectIndex && testHit.triangleIndex == hit.triangleIndex)
                break;

            if (j < numBounces - 1)
            {
                bounce(scene, j, hit, hitPoint, 0, ray, accumEnergy, accumDistance);
            }
        }
    }
}

void ReflectionSimulator::simulateJob(const IScene& scene,
                                      Array<float, 2>& image,
                                      int start,
                                      int end,
                                      int threadId)
{
    assert(0 <= threadId && threadId < mNumThreads);

    const auto scalar = 500.0f;
    const auto& camera = *mListener;
    const auto n = static_cast<int>(floorf(sqrtf(static_cast<float>(mNumRays))));

    for (auto i = start; i < end; ++i)
    {
        auto v = static_cast<float>(i / n);
        auto u = static_cast<float>(i % n);
        auto du = ((u / n) - 0.5f) * 2.0f;
        auto dv = ((v / n) - 0.5f) * 2.0f;
        auto direction = Vector3f::unitVector((camera.right * du) + (camera.up * dv) - camera.ahead);

        Ray ray{ camera.origin, direction };

        float accumEnergy[Bands::kNumBands] = { 1.0f, 1.0f, 1.0f };
        float accumDistance = 0.0f;

        for (auto j = 0; j < mNumBounces; ++j)
        {
            Hit hit;
            Vector3f hitPoint;
            if (!trace(scene, ray, j, accumDistance, hit, hitPoint))
                break;

            for (auto k = 0; k < mNumSources; ++k)
            {
                float energy[Bands::kNumBands] = { 0.0f, 0.0f, 0.0f };
                auto delay = 0.0f;

                if (!shade(scene, ray, j, k, hit, hitPoint, accumEnergy, accumDistance, scalar, energy, delay))
                    continue;

                image[i][0] += energy[0];
                image[i][1] += energy[1];
                image[i][2] += energy[2];
            }

            if (j < mNumBounces - 1)
            {
                bounce(scene, j, hit, hitPoint, threadId, ray, accumEnergy, accumDistance);
            }
        }
    }
}

void ReflectionSimulator::simulateJob(const IScene& scene,
                                      int start,
                                      int end,
                                      int threadId,
                                      std::atomic<bool>& cancel)
{
    PROFILE_FUNCTION();

    assert(0 <= threadId && threadId < mNumThreads);
    assert(0 < mNumSources && mNumSources <= mMaxNumSources);

    const auto scalar = (4.0f * Math::kPi) / mNumRays;
    const auto& listener = *mListener;

    for (auto i = start; i < end; ++i)
    {
        Ray ray{ listener.origin, mListenerSamples[i] };

        float accumEnergy[Bands::kNumBands] = { 1.0f, 1.0f, 1.0f };
        float accumDistance = 0.0f;

        for (auto j = 0; j < mNumBounces; ++j)
        {
            Hit hit;
            Vector3f hitPoint;
            if (!trace(scene, ray, j, accumDistance, hit, hitPoint))
                break;

            if (cancel)
                return;

            for (auto k = 0; k < mNumSources; ++k)
            {
                float energy[Bands::kNumBands] = { 0.0f, 0.0f, 0.0f };
                auto delay = 0.0f;

                if (!shade(scene, ray, j, k, hit, hitPoint, accumEnergy, accumDistance, scalar, energy, delay))
                    continue;

                auto& energyField = *mThreadState[threadId].energyFields[k];

                auto bin = static_cast<int>(floorf(delay / EnergyField::kBinDuration));
                if (bin < 0 || energyField.numBins() <= bin)
                    continue;

                for (auto channel = 0; channel < energyField.numChannels(); ++channel)
                {
                    for (auto band = 0; band < Bands::kNumBands; ++band)
                    {
                        energyField[channel][band][bin] += mListenerCoeffs[i][channel] * energy[band];
                    }
                }

                if (cancel)
                    return;
            }

            if (j < mNumBounces - 1)
            {
                bounce(scene, j, hit, hitPoint, threadId, ray, accumEnergy, accumDistance);

                if (cancel)
                    return;
            }
        }
    }
}

void ReflectionSimulator::finalizeJob(EnergyField* const* energyFields,
                                      std::atomic<bool>& cancel)
{
    for (auto i = 0; i < mNumSources; ++i)
    {
        auto& energyField = *energyFields[i];
        for (auto j = 0; j < mNumThreads; ++j)
        {
            EnergyField::add(energyField, *mThreadState[j].energyFields[i], energyField);
        }
    }
}

bool ReflectionSimulator::trace(const IScene& scene,
                                const Ray& ray,
                                int bounce,
                                float accumDistance,
                                Hit& hit,
                                Vector3f& hitPoint)
{
    hit = scene.closestHit(ray, 0.0f, std::numeric_limits<float>::infinity());
    if (!hit.isValid())
        return false;

    if (accumDistance > mDuration * PropagationMedium::kSpeedOfSound)
        return false;

    if (hit.distance <= kListenerRadius)
        return false;

    if (Vector3f::dot(hit.normal, ray.direction) > 0.0f)
    {
        hit.normal *= -1.0f;
    }

    hitPoint = ray.pointAtDistance(hit.distance) + (kHitSurfaceOffset * hit.normal);

    const auto& listener = *mListener;

    if (bounce > 0)
    {
        for (auto i = 0; i < mNumSources; ++i)
        {
            if ((listener.origin - mSources[i].origin).length() > kSourceRadius)
            {
                auto sourceHitDistance = ray.intersect(Sphere(mSources[i].origin, kSourceRadius));
                if (0.0f <= sourceHitDistance && sourceHitDistance < hit.distance)
                    return false;
            }
        }

        auto listenerHitDistance = ray.intersect(Sphere(listener.origin, kListenerRadius));
        if (0.0f <= listenerHitDistance && listenerHitDistance < hit.distance)
            return false;
    }

    return true;
}

bool ReflectionSimulator::shade(const IScene& scene,
                                const Ray& ray,
                                int bounce,
                                int sourceIndex,
                                const Hit& hit,
                                const Vector3f& hitPoint,
                                const float* accumEnergy,
                                float accumDistance,
                                float scalar,
                                float* energy,
                                float& delay)
{
    const auto& listener = *mListener;

    auto hitToSource = mSources[sourceIndex].origin - hitPoint;
    if (Vector3f::dot(hit.normal, hitToSource) < 0.0f)
        return false;

    auto hitToSourceDistance = hitToSource.length();
    if (hitToSourceDistance <= mIrradianceMinDistance)
        return false;

    Ray shadowRay{ hitPoint, hitToSource / hitToSourceDistance };
    if (scene.anyHit(shadowRay, 0.0f, hitToSourceDistance))
        return false;

    auto diffuseTerm = (1.0f / Math::kPi) * hit.material->scattering * std::max(Vector3f::dot(hit.normal, shadowRay.direction), 0.0f);
    auto halfVector = Vector3f::unitVector((shadowRay.direction - ray.direction) * 0.5f);
    auto specularTerm = ((kSpecularExponent + 2.0f) / (8.0f * Math::kPi)) * (1.0f - hit.material->scattering) * powf(Vector3f::dot(halfVector, hit.normal), kSpecularExponent);
    auto attenuation = 1.0f / std::max(hitToSourceDistance, mIrradianceMinDistance);
    auto distanceTerm = (1.0f / (4.0f * Math::kPi)) * (attenuation * attenuation);
    auto directivityTerm = mDirectivities[sourceIndex].evaluateAt(hitPoint, mSources[sourceIndex]);
    auto frequencyIndependentTerm = scalar * distanceTerm * directivityTerm * (diffuseTerm + specularTerm);

    for (auto j = 0; j < Bands::kNumBands; ++j)
    {
        energy[j] = frequencyIndependentTerm * (1.0f - hit.material->absorption[j]) * accumEnergy[j];
    }

    auto distance = accumDistance + hit.distance + hitToSourceDistance;
    delay = (distance / PropagationMedium::kSpeedOfSound) - DirectSimulator::directPathDelay(listener.origin, mSources[sourceIndex].origin);

    return true;
}

void ReflectionSimulator::bounce(const IScene& scene,
                                 int bounce,
                                 const Hit& hit,
                                 const Vector3f& hitPoint,
                                 int threadId,
                                 Ray& ray,
                                 float* accumEnergy,
                                 float& accumDistance)
{
    for (auto j = 0; j < Bands::kNumBands; ++j)
    {
        accumEnergy[j] *= (1.0f - hit.material->absorption[j]);
    }

    accumDistance += hit.distance;

    ray.origin = hitPoint;

    if (mThreadState[threadId].rng.uniformRandomNormalized() < hit.material->scattering)
    {
        auto diffuseSampleIndex = mThreadState[threadId].rng.uniformRandom() % mNumDiffuseSamples;
        ray.direction = Sampling::transformHemisphereSample(mDiffuseSamples[diffuseSampleIndex], hit.normal);
    }
    else
    {
        ray.direction = Vector3f::reflect(ray.direction, hit.normal);
    }
}


// --------------------------------------------------------------------------------------------------------------------
// BatchedReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

BatchedReflectionSimulator::BatchedReflectionSimulator(int maxNumRays,
                                                       int numDiffuseSamples,
                                                       float maxDuration,
                                                       int maxOrder,
                                                       int maxNumSources,
                                                       int numThreads,
                                                       int rayBatchSize)
    : mMaxNumRays(maxNumRays)
    , mNumDiffuseSamples(numDiffuseSamples)
    , mMaxDuration(maxDuration)
    , mMaxOrder(maxOrder)
    , mMaxNumSources(maxNumSources)
    , mNumThreads(numThreads)
    , mRayBatchSize(rayBatchSize)
    , mNumSources(maxNumSources)
    , mSources(nullptr)
    , mListener(nullptr)
    , mDirectivities(nullptr)
    , mNumRays(maxNumRays)
    , mNumBounces(0)
    , mDuration(maxDuration)
    , mOrder(maxOrder)
    , mIrradianceMinDistance(1.0f)
    , mListenerSamples(maxNumRays)
    , mDiffuseSamples(numDiffuseSamples)
    , mListenerCoeffs(maxNumRays, SphericalHarmonics::numCoeffsForOrder(maxOrder))
    , mThreadState(numThreads)
{
    Sampling::generateSphereSamples(maxNumRays, mListenerSamples.data());
    Sampling::generateHemisphereSamples(numDiffuseSamples, mDiffuseSamples.data());

    for (auto i = 0; i < maxNumRays; ++i)
    {
        for (auto l = 0, j = 0; l <= maxOrder; ++l)
        {
            for (auto m = -l; m <= l; ++m, ++j)
            {
                mListenerCoeffs[i][j] = SphericalHarmonics::evaluate(l, m, mListenerSamples[i]);
            }
        }
    }

    for (auto i = 0; i < numThreads; ++i)
    {
        mThreadState[i].rays.resize(rayBatchSize);
        mThreadState[i].minDistances.resize(rayBatchSize);
        mThreadState[i].maxDistances.resize(rayBatchSize);
        mThreadState[i].hits.resize(rayBatchSize);
        mThreadState[i].shadowRays.resize(rayBatchSize);
        mThreadState[i].shadowRayMinDistances.resize(rayBatchSize);
        mThreadState[i].shadowRayMaxDistances.resize(rayBatchSize);
        mThreadState[i].occluded.resize(rayBatchSize);
        mThreadState[i].escaped.resize(rayBatchSize);
        mThreadState[i].hitPoints.resize(rayBatchSize);
        mThreadState[i].energy.resize(rayBatchSize, Bands::kNumBands);
        mThreadState[i].delay.resize(rayBatchSize);
        mThreadState[i].accumEnergy.resize(rayBatchSize, Bands::kNumBands);
        mThreadState[i].accumDistance.resize(rayBatchSize);

        mThreadState[i].energyFields.resize(maxNumSources);
        for (auto j = 0; j < maxNumSources; ++j)
        {
            mThreadState[i].energyFields[j] = make_unique<EnergyField>(maxDuration, maxOrder);
        }
    }
}

void BatchedReflectionSimulator::simulate(const IScene& scene,
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
                                          JobGraph& jobGraph)
{
    PROFILE_FUNCTION();

    assert(numListeners == 1);

    // If we've been asked to simulate more sources than the max number we were initialized with, don't process the
    // extra ones.
    if (numSources > mMaxNumSources)
    {
        gLog().message(MessageSeverity::Warning,
            "Simulating reflections for %d sources, which is more than the max (%d). Some sources will be ignored.",
            numSources, mMaxNumSources);

        numSources = mMaxNumSources;
    }

    mNumSources = numSources;
    mSources = sources;
    mListener = &listeners[0];
    mDirectivities = directivities;
    mNumRays = numRays;
    mNumBounces = numBounces;
    mDuration = duration;
    mOrder = order;
    mIrradianceMinDistance = irradianceMinDistance;

    image.zero();

    for (auto i = 0; i < numRays; i += mRayBatchSize)
    {
        auto start = i;
        auto end = std::min(numRays, i + mRayBatchSize);

        jobGraph.addJob([this, &scene, &image, start, end](int threadId, std::atomic<bool>& cancel)
        {
            simulateJob(scene, image, start, end, threadId);
        });
    }
}

void BatchedReflectionSimulator::simulate(const IScene& scene,
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
                                          JobGraph& jobGraph)
{
    PROFILE_FUNCTION();

    assert(numListeners == 1);

    // If we've been asked to simulate more sources than the max number we were initialized with, don't process the
    // extra ones.
    if (numSources > mMaxNumSources)
    {
        gLog().message(MessageSeverity::Warning,
            "Simulating reflections for %d sources, which is more than the max (%d). Some sources will be ignored.",
            numSources, mMaxNumSources);

        numSources = mMaxNumSources;
    }

    mNumSources = numSources;
    mSources = sources;
    mListener = &listeners[0];
    mDirectivities = directivities;
    mNumRays = numRays;
    mNumBounces = numBounces;
    mDuration = duration;
    mOrder = order;
    mIrradianceMinDistance = irradianceMinDistance;

    for (auto i = 0; i < numSources; ++i)
    {
        energyFields[i]->reset();
        for (auto j = 0; j < mNumThreads; ++j)
        {
            mThreadState[j].energyFields[i]->reset();
        }
    }

    mNumJobsRemaining = 0;

    for (auto i = 0; i < numRays; i += mRayBatchSize)
    {
        ++mNumJobsRemaining;

        auto start = i;
        auto end = std::min(numRays, i + mRayBatchSize);

        jobGraph.addJob([this, &scene, energyFields, start, end](int threadId, std::atomic<bool>& cancel)
        {
            simulateJob(scene, start, end, threadId, cancel);

            if (--mNumJobsRemaining == 0)
            {
                finalizeJob(energyFields, cancel);
            }
        });
    }
}

void BatchedReflectionSimulator::simulate(const IScene& scene,
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
                                          vector<Ray>& escapedRays)
{}

void BatchedReflectionSimulator::simulateJob(const IScene& scene,
                                             Array<float, 2>& image,
                                             int start,
                                             int end,
                                             int threadId)
{
    assert(0 <= threadId && threadId < mNumThreads);

    const auto scalar = 500.0f;
    const auto& camera = *mListener;
    const auto n = static_cast<int>(floorf(sqrtf(static_cast<float>(mNumRays))));

    reset(threadId);

    for (auto i = start; i < end; ++i)
    {
        auto v = static_cast<float>(i / n);
        auto u = static_cast<float>(i % n);
        auto du = ((u / n) - 0.5f) * 2.0f;
        auto dv = ((v / n) - 0.5f) * 2.0f;
        auto direction = Vector3f::unitVector((camera.right * du) + (camera.up * dv) - camera.ahead);

        mThreadState[threadId].rays[i] = Ray{ camera.origin, direction };
    }

    for (auto i = 0; i < mNumBounces; ++i)
    {
        if (!trace(scene, i, start, end, threadId))
            break;

        for (auto j = 0; j < mNumSources; ++j)
        {
            shade(scene, i, j, scalar, start, end, threadId);

            for (auto k = start; k < end; ++k)
            {
                if (!mThreadState[threadId].occluded[k])
                {
                    image[k][0] += mThreadState[threadId].energy[k - start][0];
                    image[k][1] += mThreadState[threadId].energy[k - start][1];
                    image[k][2] += mThreadState[threadId].energy[k - start][2];
                }
            }
        }

        if (i < mNumBounces - 1)
        {
            bounce(scene, i, start, end, threadId);
        }
    }
}

void BatchedReflectionSimulator::simulateJob(const IScene& scene,
                                             int start,
                                             int end,
                                             int threadId,
                                             std::atomic<bool>& cancel)
{
    PROFILE_FUNCTION();

    assert(0 <= threadId && threadId < mNumThreads);

    const auto scalar = (4.0f * Math::kPi) / mNumRays;
    const auto& listener = *mListener;

    reset(threadId);

    for (auto i = start; i < end; ++i)
    {
        mThreadState[threadId].rays[i - start] = Ray{ listener.origin, mListenerSamples[i] };
    }

    for (auto i = 0; i < mNumBounces; ++i)
    {
        if (!trace(scene, i, start, end, threadId))
            break;

        if (cancel)
            return;

        for (auto j = 0; j < mNumSources; ++j)
        {
            shade(scene, i, j, scalar, start, end, threadId);

            if (cancel)
                return;

            auto& energyField = *mThreadState[threadId].energyFields[j];

            for (auto k = start; k < end; ++k)
            {
                if (mThreadState[threadId].occluded[k - start])
                    continue;

                auto bin = static_cast<int>(floorf(mThreadState[threadId].delay[k - start] / EnergyField::kBinDuration));
                if (bin < 0 || energyField.numBins() <= bin)
                    continue;

                for (auto channel = 0; channel < energyField.numChannels(); ++channel)
                {
                    for (auto band = 0; band < Bands::kNumBands; ++band)
                    {
                        energyField[channel][band][bin] += mListenerCoeffs[k][channel] * mThreadState[threadId].energy[k - start][band];
                    }
                }
            }

            if (cancel)
                return;
        }

        if (i < mNumBounces - 1)
        {
            bounce(scene, i, start, end, threadId);

            if (cancel)
                return;
        }
    }
}

void BatchedReflectionSimulator::finalizeJob(EnergyField* const* energyFields,
                                             std::atomic<bool>& cancel)
{
    for (auto i = 0; i < mNumSources; ++i)
    {
        auto& energyField = *energyFields[i];
        for (auto j = 0; j < mNumThreads; ++j)
        {
            EnergyField::add(energyField, *mThreadState[j].energyFields[i], energyField);
        }
    }
}

void BatchedReflectionSimulator::reset(int threadId)
{
    for (auto i = 0; i < mRayBatchSize; ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mThreadState[threadId].accumEnergy[i][j] = 1.0f;
        }
    }

    mThreadState[threadId].accumDistance.zero();

    mThreadState[threadId].escaped.zero();
}

bool BatchedReflectionSimulator::trace(const IScene& scene,
                                       int bounce,
                                       int start,
                                       int end,
                                       int threadId)
{
    for (auto i = start; i < end; ++i)
    {
        mThreadState[threadId].minDistances[i - start] = 0.0f;
        mThreadState[threadId].maxDistances[i - start] = std::numeric_limits<float>::infinity();
    }

    auto numRays = end - start;
    auto& threadState = mThreadState[threadId];

    scene.closestHits(numRays, threadState.rays.data(), threadState.minDistances.data(), threadState.maxDistances.data(), threadState.hits.data());

    auto numEscaped = 0;

    for (auto i = start; i < end; ++i)
    {
        if (escaped(scene, bounce, threadState.rays[i - start], threadState.hits[i - start], threadState.accumDistance[i - start]))
        {
            threadState.escaped[i - start] = true;
            ++numEscaped;
        }
        else
        {
            if (Vector3f::dot(threadState.hits[i - start].normal, threadState.rays[i - start].direction) > 0.0f)
            {
                threadState.hits[i - start].normal *= -1.0f;
            }

            threadState.hitPoints[i - start] = threadState.rays[i - start].pointAtDistance(threadState.hits[i - start].distance) + (kHitSurfaceOffset * threadState.hits[i - start].normal);
        }
    }

    if (numEscaped >= numRays)
        return false;

    return true;
}

bool BatchedReflectionSimulator::escaped(const IScene& scene,
                                         int bounce,
                                         const Ray& ray,
                                         const Hit& hit,
                                         float accumDistance)
{
    if (!hit.isValid())
        return true;

    if (accumDistance > mDuration * PropagationMedium::kSpeedOfSound)
        return true;

    if (hit.distance <= kListenerRadius)
        return true;

    if (bounce > 0)
    {
        for (auto i = 0; i < mNumSources; ++i)
        {
            if ((mListener->origin - mSources[i].origin).length() > kSourceRadius)
            {
                auto sourceHitDistance = ray.intersect(Sphere(mSources[i].origin, kSourceRadius));
                if (0.0f <= sourceHitDistance && sourceHitDistance < hit.distance)
                    return true;
            }
        }

        auto listenerHitDistance = ray.intersect(Sphere(mListener->origin, kListenerRadius));
        if (0.0f <= listenerHitDistance && listenerHitDistance < hit.distance)
            return true;
    }

    return false;
}

void BatchedReflectionSimulator::shade(const IScene& scene,
                                       int bounce,
                                       int sourceIndex,
                                       float scalar,
                                       int start,
                                       int end,
                                       int threadId)
{
    auto numRays = end - start;
    auto& threadState = mThreadState[threadId];

    for (auto i = start; i < end; ++i)
    {
        auto hitToSource = mSources[sourceIndex].origin - threadState.hitPoints[i - start];
        auto hitToSourceDistance = hitToSource.length();

        if (threadState.escaped[i - start] ||
            hitToSourceDistance <= mIrradianceMinDistance ||
            Vector3f::dot(threadState.hits[i - start].normal, hitToSource) < 0.0f)
        {
            threadState.shadowRays[i - start] = Ray{ Vector3f::kZero, Vector3f::kXAxis };
            threadState.shadowRayMinDistances[i - start] = 0.0f;
            threadState.shadowRayMaxDistances[i - start] = -1.0f;
        }
        else
        {
            threadState.shadowRays[i - start] = Ray{ threadState.hitPoints[i - start], hitToSource / hitToSourceDistance };
            threadState.shadowRayMinDistances[i - start] = 0.0f;
            threadState.shadowRayMaxDistances[i - start] = hitToSourceDistance;
        }
    }

    scene.anyHits(numRays, threadState.shadowRays.data(), threadState.shadowRayMinDistances.data(), threadState.shadowRayMaxDistances.data(), threadState.occluded.data());

    for (auto i = start; i < end; ++i)
    {
        if (threadState.occluded[i - start])
            continue;

        auto diffuseTerm = (1.0f / Math::kPi) * threadState.hits[i - start].material->scattering * std::max(Vector3f::dot(threadState.hits[i - start].normal, threadState.shadowRays[i - start].direction), 0.0f);
        auto halfVector = Vector3f::unitVector((threadState.shadowRays[i - start].direction - threadState.rays[i - start].direction) * 0.5f);
        auto specularTerm = ((kSpecularExponent + 2.0f) / (8.0f * Math::kPi)) * (1.0f - threadState.hits[i - start].material->scattering) * powf(Vector3f::dot(halfVector, threadState.hits[i - start].normal), kSpecularExponent);
        auto attenuation = 1.0f / std::max(threadState.shadowRayMaxDistances[i - start], mIrradianceMinDistance);
        auto distanceTerm = (1.0f / (4.0f * Math::kPi)) * (attenuation * attenuation);
        auto directivityTerm = mDirectivities[sourceIndex].evaluateAt(threadState.hitPoints[i - start], mSources[sourceIndex]);
        auto frequencyIndependentTerm = scalar * distanceTerm * directivityTerm * (diffuseTerm + specularTerm);

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            threadState.energy[i - start][j] = frequencyIndependentTerm * (1.0f - threadState.hits[i - start].material->absorption[j]) * threadState.accumEnergy[i - start][j];
        }

        auto distance = threadState.accumDistance[i - start] + threadState.hits[i - start].distance + threadState.shadowRayMaxDistances[i - start];
        threadState.delay[i - start] = (distance / PropagationMedium::kSpeedOfSound) - DirectSimulator::directPathDelay(mListener->origin, mSources[sourceIndex].origin);
    }
}

void BatchedReflectionSimulator::bounce(const IScene& scene,
                                        int bounce,
                                        int start,
                                        int end,
                                        int threadId)
{
    auto numRays = end - start;
    auto& threadState = mThreadState[threadId];

    for (auto i = start; i < end; ++i)
    {
        if (threadState.escaped[i - start])
            continue;

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            threadState.accumEnergy[i - start][j] *= (1.0f - threadState.hits[i - start].material->absorption[j]);
        }

        threadState.accumDistance[i - start] += threadState.hits[i - start].distance;

        threadState.rays[i - start].origin = threadState.hitPoints[i - start];

        if (threadState.rng.uniformRandomNormalized() < threadState.hits[i - start].material->scattering)
        {
            auto diffuseSampleIndex = threadState.rng.uniformRandom() % mNumDiffuseSamples;
            threadState.rays[i - start].direction = Sampling::transformHemisphereSample(mDiffuseSamples[diffuseSampleIndex], threadState.hits[i - start].normal);
        }
        else
        {
            threadState.rays[i - start].direction = Vector3f::reflect(threadState.rays[i - start].direction, threadState.hits[i - start].normal);
        }
    }
}

}
