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

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#include "embree_reflection_simulator.h"

#include "propagation_medium.h"
#include "sh.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

const int EmbreeReflectionSimulator::kRayBatchSize = 64;
const int EmbreeReflectionSimulator::kBlockSize = 8;

EmbreeReflectionSimulator::EmbreeReflectionSimulator(int maxNumRays,
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
    , mSources(maxNumSources)
    , mListener(1)
    , mDirectivities(maxNumSources)
    , mEnergyFields(numThreads * maxNumSources)
    , mNumRays(maxNumRays)
    , mNumBounces(0)
    , mDuration(maxDuration)
    , mOrder(maxOrder)
    , mIrradianceMinDistance(1.0f)
    , mListenerSamples(3, maxNumRays)
    , mDiffuseSamples(3, numDiffuseSamples)
    , mListenerCoeffs(SphericalHarmonics::numCoeffsForOrder(maxOrder), maxNumRays)
    , mThreadState(numThreads)
{
    Array<Vector3f> listenerSamples(maxNumRays);
    Array<Vector3f> diffuseSamples(numDiffuseSamples);

    Sampling::generateSphereSamples(maxNumRays, listenerSamples.data());
    Sampling::generateHemisphereSamples(numDiffuseSamples, diffuseSamples.data());

    for (auto i = 0; i < maxNumRays; ++i)
    {
        for (auto j = 0; j < 3; ++j)
        {
            mListenerSamples[j][i] = listenerSamples[i].elements[j];
        }

        for (auto l = 0, j = 0; l <= maxOrder; ++l)
        {
            for (auto m = -l; m <= l; ++m, ++j)
            {
                mListenerCoeffs[j][i] = SphericalHarmonics::evaluate(l, m, listenerSamples[i]);
            }
        }
    }

    for (auto i = 0; i < numDiffuseSamples; ++i)
    {
        for (auto j = 0; j < 3; ++j)
        {
            mDiffuseSamples[j][i] = diffuseSamples[i].elements[j];
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

void EmbreeReflectionSimulator::simulate(const IScene& scene,
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
    mListener[0] = ispcCoordinateSpace(listeners[0]);
    mNumRays = numRays;
    mNumBounces = numBounces;
    mDuration = duration;
    mOrder = order;
    mIrradianceMinDistance = irradianceMinDistance;

    for (auto i = 0; i < numSources; ++i)
    {
        mSources[i] = ispcCoordinateSpace(sources[i]);
        mDirectivities[i] = ispcDirectivity(directivities[i]);
    }

    mScene = ispcEmbreeScene(static_cast<const EmbreeScene&>(scene));
    mReflectionSimulator = ispcEmbreeReflectionSimulator(*this);

    image.zero();

    auto imageSize = static_cast<int>(floorf(sqrtf(static_cast<float>(numRays))));

    for (auto xIndex = 0; xIndex < imageSize; xIndex += kBlockSize)
    {
        for (auto yIndex = 0; yIndex < imageSize; yIndex += kBlockSize)
        {
            jobGraph.addJob([this, xIndex, yIndex, imageSize, &image](int threadId, std::atomic<bool>& cancel)
            {
                ispc::simulateImage(&mScene, &mReflectionSimulator, xIndex, yIndex, kBlockSize, imageSize, image.flatData());
            });
        }
    }
}

void EmbreeReflectionSimulator::simulate(const IScene& scene,
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
    mListener[0] = ispcCoordinateSpace(listeners[0]);
    mNumRays = numRays;
    mNumBounces = numBounces;
    mDuration = duration;
    mOrder = order;
    mIrradianceMinDistance = irradianceMinDistance;

    for (auto i = 0; i < numSources; ++i)
    {
        mSources[i] = ispcCoordinateSpace(sources[i]);
        mDirectivities[i] = ispcDirectivity(directivities[i]);
    }

    for (auto i = 0, index = 0; i < mNumThreads; ++i)
    {
        for (auto j = 0; j < numSources; ++j, ++index)
        {
            mEnergyFields[index] = ispcEnergyField(*mThreadState[i].energyFields[j].get());
        }
    }

    mScene = ispcEmbreeScene(static_cast<const EmbreeScene&>(scene));
    mReflectionSimulator = ispcEmbreeReflectionSimulator(*this);

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

        jobGraph.addJob([this, energyFields, start, end](int threadId, std::atomic<bool>& cancel)
        {
            PROFILE_ZONE("ispc::simulateEnergyField");
            ispc::simulateEnergyField(&mScene, &mReflectionSimulator, start, end, threadId, mEnergyFields.data());

            if (--mNumJobsRemaining == 0)
            {
                for (auto j = 0; j < mNumSources; ++j)
                {
                    auto& energyField = *energyFields[j];
                    for (auto k = 0; k < mNumThreads; ++k)
                    {
                        EnergyField::add(energyField, *mThreadState[k].energyFields[j], energyField);
                    }
                }
            }
        });
    }
}

void EmbreeReflectionSimulator::simulate(const IScene& scene,
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

ispc::CoordinateSpace EmbreeReflectionSimulator::ispcCoordinateSpace(const CoordinateSpace3f& in)
{
    ispc::CoordinateSpace out;
    out.right.v[0] = in.right.elements[0];
    out.right.v[1] = in.right.elements[1];
    out.right.v[2] = in.right.elements[2];
    out.up.v[0] = in.up.elements[0];
    out.up.v[1] = in.up.elements[1];
    out.up.v[2] = in.up.elements[2];
    out.ahead.v[0] = in.ahead.elements[0];
    out.ahead.v[1] = in.ahead.elements[1];
    out.ahead.v[2] = in.ahead.elements[2];
    out.origin.v[0] = in.origin.elements[0];
    out.origin.v[1] = in.origin.elements[1];
    out.origin.v[2] = in.origin.elements[2];
    return out;
}

ispc::Directivity EmbreeReflectionSimulator::ispcDirectivity(const Directivity& in)
{
    return reinterpret_cast<const ispc::Directivity&>(in);
}

ispc::Material EmbreeReflectionSimulator::ispcMaterial(const Material& in)
{
    return reinterpret_cast<const ispc::Material&>(in);
}

ispc::EnergyField EmbreeReflectionSimulator::ispcEnergyField(EnergyField& in)
{
    ispc::EnergyField out;
    out.numChannels = in.numChannels();
    out.numBins = in.numBins();
    out.data = in.data();
    return out;
}

ispc::EmbreeScene EmbreeReflectionSimulator::ispcEmbreeScene(const EmbreeScene& in)
{
    ispc::EmbreeScene out;
    out.scene = in.scene();
    out.materialsForGeometry = reinterpret_cast<const ispc::Material* const*>(in.materialsForGeometry());
    out.materialIndicesForGeometry = in.materialIndicesForGeometry();
    return out;
}

ispc::EmbreeReflectionSimulator EmbreeReflectionSimulator::ispcEmbreeReflectionSimulator(const EmbreeReflectionSimulator& in)
{
    ispc::EmbreeReflectionSimulator out;
    out.speedOfSound = PropagationMedium::kSpeedOfSound;
    out.maxNumRays = in.mMaxNumRays;
    out.numDiffuseSamples = in.mNumDiffuseSamples;
    out.maxDuration = in.mMaxDuration;
    out.maxOrder = in.mMaxOrder;
    out.maxNumSources = in.mMaxNumSources;
    out.numSources = in.mNumSources;
    out.sources = in.mSources.data();
    out.listener = &in.mListener[0];
    out.directivities = in.mDirectivities.data();
    out.numRays = in.mNumRays;
    out.numBounces = in.mNumBounces;
    out.duration = in.mDuration;
    out.order = in.mOrder;
    out.irradianceMinDistance = in.mIrradianceMinDistance;
    out.listenerSamples = in.mListenerSamples.data();
    out.diffuseSamples = in.mDiffuseSamples.data();
    out.listenerCoeffs = in.mListenerCoeffs.data();
    return out;
}

}

#endif