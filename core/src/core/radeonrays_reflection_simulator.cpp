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

#if defined(IPL_USES_RADEONRAYS)

#include "radeonrays_reflection_simulator.h"

#include "sh.h"
#include "radeonrays_static_mesh.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysReflectionSimulator
// --------------------------------------------------------------------------------------------------------------------

namespace cl {

struct CoordinateSpace
{
    cl_float3 right;
    cl_float3 up;
    cl_float3 ahead;
    cl_float3 origin;
};

struct Directivity
{
    cl_float dipoleWeight;
    cl_float dipolePower;
};

}

const float RadeonRaysReflectionSimulator::kHistogramScale = 1e+10f;

RadeonRaysReflectionSimulator::RadeonRaysReflectionSimulator(int maxNumRays,
                                                             int numDiffuseSamples,
                                                             float maxDuration,
                                                             int maxOrder,
                                                             int maxNumSources,
                                                             int maxNumListeners,
                                                             shared_ptr<RadeonRaysDevice> radeonRays)
    : mRadeonRays(radeonRays)
    , mMaxNumRays(maxNumRays)
    , mNumDiffuseSamples(numDiffuseSamples)
    , mMaxDuration(maxDuration)
    , mMaxOrder(maxOrder)
    , mMaxNumSources(maxNumSources)
    , mMaxNumListeners(maxNumListeners)
    , mSources(radeonRays->openCL(), maxNumSources * sizeof(cl::CoordinateSpace))
    , mListeners(radeonRays->openCL(), maxNumListeners * sizeof(cl::CoordinateSpace))
    , mDirectivities(radeonRays->openCL(), maxNumSources * sizeof(cl::Directivity))
    , mListenerSamples(radeonRays->openCL(), maxNumRays * sizeof(cl_float4))
    , mDiffuseSamples(radeonRays->openCL(), numDiffuseSamples * sizeof(cl_float4))
    , mListenerCoeffs(radeonRays->openCL(), maxNumRays * SphericalHarmonics::numCoeffsForOrder(maxOrder) * sizeof(cl_float))
    , mEnergy(radeonRays->openCL(), std::max(maxNumSources, maxNumListeners) * maxNumRays * sizeof(cl_float4))
    , mAccumEnergy(radeonRays->openCL(), std::max(maxNumSources, maxNumListeners) * maxNumRays * sizeof(cl_float4))
    , mImage(radeonRays->openCL(), maxNumRays * sizeof(cl_float4))
    , mRays{ RadeonRaysBuffer(radeonRays, maxNumListeners * maxNumRays * sizeof(RadeonRays::ray)), RadeonRaysBuffer(radeonRays, maxNumListeners * maxNumRays * sizeof(RadeonRays::ray)) }
    , mCurrentRayBuffer(0)
    , mNumRays(radeonRays, sizeof(cl_int))
    , mHits(radeonRays, maxNumListeners * maxNumRays * sizeof(RadeonRays::Intersection))
    , mShadowRays(radeonRays, std::max(maxNumSources, maxNumListeners) * maxNumRays * sizeof(RadeonRays::ray))
    , mNumShadowRays(radeonRays, sizeof(cl_int))
    , mOccluded(radeonRays, std::max(maxNumSources, maxNumListeners) * maxNumRays * sizeof(cl_int))
    , mGenerateCameraRays(radeonRays->openCL(), radeonRays->program(), "generateCameraRays")
    , mGenerateListenerRays(radeonRays->openCL(), radeonRays->program(), "generateListenerRays")
    , mSphereOcclusion(radeonRays->openCL(), radeonRays->program(), "sphereOcclusion")
    , mShadeAndBounce(radeonRays->openCL(), radeonRays->program(), "shadeAndBounce")
    , mGatherImage(radeonRays->openCL(), radeonRays->program(), "gatherImage")
    , mGatherEnergyField(radeonRays->openCL(), radeonRays->program(), "gatherEnergyField")
    , mShadeLocalSize(256)
{
    assert(maxNumListeners == 1 || maxNumSources == 1 || (maxNumListeners == maxNumSources));

    Array<Vector3f> listenerSamples(maxNumRays);
    Array<Vector3f> diffuseSamples(numDiffuseSamples);
    Sampling::generateSphereSamples(maxNumRays, listenerSamples.data());
    Sampling::generateHemisphereSamples(numDiffuseSamples, diffuseSamples.data());

    auto status = CL_SUCCESS;
    auto* _listenerSamples = reinterpret_cast<Vector4f*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                                         mListenerSamples.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                                         0, mListenerSamples.size(), 0, nullptr, nullptr, &status));
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    auto* _diffuseSamples = reinterpret_cast<Vector4f*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                                        mDiffuseSamples.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                                        0, mDiffuseSamples.size(), 0, nullptr, nullptr, &status));
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    auto* _listenerCoeffs = reinterpret_cast<float*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                                     mListenerCoeffs.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                                     0, mListenerCoeffs.size(), 0, nullptr, nullptr, &status));
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    for (auto i = 0; i < maxNumRays; ++i)
    {
        _listenerSamples[i] = listenerSamples[i];
    }

    for (auto i = 0; i < numDiffuseSamples; ++i)
    {
        _diffuseSamples[i] = diffuseSamples[i];
    }

    for (auto l = 0, i = 0; l <= maxOrder; ++l)
    {
        for (auto m = -l; m <= l; ++m)
        {
            for (auto j = 0; j < maxNumRays; ++j, ++i)
            {
                _listenerCoeffs[i] = SphericalHarmonics::evaluate(l, m, listenerSamples[j]);
            }
        }
    }

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mListenerCoeffs.buffer(), _listenerCoeffs,
                            0, nullptr, nullptr);

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mDiffuseSamples.buffer(), _diffuseSamples,
                            0, nullptr, nullptr);

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mListenerSamples.buffer(), _listenerSamples,
                            0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::simulate(const IScene& scene,
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

    image.zero();

    reset();

    // If the scene is empty, stop here. Radeon Rays crashes if QueryIntersection or QueryOcclusion are called with
    // an empty scene.
    if (mRadeonRays->api()->IsWorldEmpty())
        return;

    setSourcesAndListeners(numSources, sources, numListeners, listeners, directivities);
    setNumRays(numSources, numListeners, numRays);

    generateCameraRays(numRays);

    for (auto i = 0; i < numBounces; ++i)
    {
        mRadeonRays->api()->QueryIntersection(mRays[mCurrentRayBuffer].rrBuffer(), mNumRays.rrBuffer(),
                                              mMaxNumListeners * mMaxNumRays, mHits.rrBuffer(), nullptr, nullptr);

        if (i > 0)
        {
            sphereOcclusion(numSources, sources, numListeners, listeners, numRays);
        }

        shadeAndBounce(static_cast<const RadeonRaysScene&>(scene), numSources, sources, numListeners, listeners,
                       directivities, numRays, numBounces, irradianceMinDistance, 500.0f);

        if (i < numBounces - 1)
        {
            mCurrentRayBuffer = 1 - mCurrentRayBuffer;
        }

        mRadeonRays->api()->QueryOcclusion(mShadowRays.rrBuffer(), mNumShadowRays.rrBuffer(),
                                           mMaxNumSources * mMaxNumRays, mOccluded.rrBuffer(), nullptr, nullptr);

        gatherImage(numSources, numRays);

        clFlush(mRadeonRays->openCL().irUpdateQueue());
    }

    clEnqueueReadBuffer(mRadeonRays->openCL().irUpdateQueue(), mImage.buffer(), CL_TRUE, 0, mImage.size(),
                        image.flatData(), 0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::simulate(const IScene& scene,
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

    // If we've been asked to simulate more sources than the max number we were initialized with, don't process the
    // extra ones.
    if (numSources > mMaxNumSources)
    {
        gLog().message(MessageSeverity::Warning,
            "Simulating reflections for %d sources, which is more than the max (%d). Some sources will be ignored.",
            numSources, mMaxNumSources);

        numSources = mMaxNumSources;
    }

    for (auto i = 0; i < std::max(numSources, numListeners); ++i)
    {
        energyFields[i]->reset();
    }

    reset();

    // If the scene is empty, stop here. Radeon Rays crashes if QueryIntersection or QueryOcclusion are called with
    // an empty scene.
    if (mRadeonRays->api()->IsWorldEmpty())
        return;

    setSourcesAndListeners(numSources, sources, numListeners, listeners, directivities);
    setNumRays(numSources, numListeners, numRays);

    generateListenerRays(numListeners, numRays);

    for (auto i = 0; i < numBounces; ++i)
    {
        mRadeonRays->api()->QueryIntersection(mRays[mCurrentRayBuffer].rrBuffer(), mNumRays.rrBuffer(),
                                              mMaxNumListeners * mMaxNumRays, mHits.rrBuffer(), nullptr, nullptr);

        if (i > 0)
        {
            sphereOcclusion(numSources, sources, numListeners, listeners, numRays);
        }

        shadeAndBounce(static_cast<const RadeonRaysScene&>(scene), numSources, sources, numListeners, listeners,
                       directivities, numRays, numBounces, irradianceMinDistance, (4.0f * Math::kPi) / numRays);

        if (i < numBounces - 1)
        {
            mCurrentRayBuffer = 1 - mCurrentRayBuffer;
        }

        mRadeonRays->api()->QueryOcclusion(mShadowRays.rrBuffer(), mNumShadowRays.rrBuffer(),
                                           std::max(mMaxNumSources, mMaxNumListeners) * mMaxNumRays, mOccluded.rrBuffer(), nullptr, nullptr);

        for (auto j = 0; j < std::max(numSources, numListeners); ++j)
        {
            gatherEnergyField(j, numRays, *static_cast<OpenCLEnergyField*>(energyFields[j]));
        }

        clFlush(mRadeonRays->openCL().irUpdateQueue());
    }
}

void RadeonRaysReflectionSimulator::simulate(const IScene& scene,
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

void RadeonRaysReflectionSimulator::reset()
{
    auto zero = 0.0f;
    auto accum = cl_float4{ 1.0f, 1.0f, 1.0f, 0.0f };

    clEnqueueFillBuffer(mRadeonRays->openCL().irUpdateQueue(), mImage.buffer(), &zero, sizeof(cl_float), 0,
                        mImage.size(), 0, nullptr, nullptr);

    clEnqueueFillBuffer(mRadeonRays->openCL().irUpdateQueue(), mAccumEnergy.buffer(), &accum, sizeof(cl_float4), 0,
                        mAccumEnergy.size(), 0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::setSourcesAndListeners(int numSources,
                                                           const CoordinateSpace3f* sources,
                                                           int numListeners,
                                                           const CoordinateSpace3f* listeners,
                                                           const Directivity* directivities)
{
    auto* _sources = reinterpret_cast<cl::CoordinateSpace*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                                            mSources.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                                            0, mSources.size(), 0, nullptr, nullptr, nullptr));

    auto* _listeners = reinterpret_cast<cl::CoordinateSpace*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                                              mListeners.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                                              0, mListeners.size(), 0, nullptr, nullptr, nullptr));

    auto* _directivities = reinterpret_cast<cl::Directivity*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                                              mDirectivities.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                                              0, mDirectivities.size(), 0, nullptr, nullptr, nullptr));

    for (auto i = 0; i < numSources; ++i)
    {
        _sources[i].right.x = sources[i].right.x();
        _sources[i].right.y = sources[i].right.y();
        _sources[i].right.z = sources[i].right.z();
        _sources[i].up.x = sources[i].up.x();
        _sources[i].up.y = sources[i].up.y();
        _sources[i].up.z = sources[i].up.z();
        _sources[i].ahead.x = sources[i].ahead.x();
        _sources[i].ahead.y = sources[i].ahead.y();
        _sources[i].ahead.z = sources[i].ahead.z();
        _sources[i].origin.x = sources[i].origin.x();
        _sources[i].origin.y = sources[i].origin.y();
        _sources[i].origin.z = sources[i].origin.z();
    }

    for (auto i = 0; i < numListeners; ++i)
    {
        _listeners[i].right.x = listeners[i].right.x();
        _listeners[i].right.y = listeners[i].right.y();
        _listeners[i].right.z = listeners[i].right.z();
        _listeners[i].up.x = listeners[i].up.x();
        _listeners[i].up.y = listeners[i].up.y();
        _listeners[i].up.z = listeners[i].up.z();
        _listeners[i].ahead.x = listeners[i].ahead.x();
        _listeners[i].ahead.y = listeners[i].ahead.y();
        _listeners[i].ahead.z = listeners[i].ahead.z();
        _listeners[i].origin.x = listeners[i].origin.x();
        _listeners[i].origin.y = listeners[i].origin.y();
        _listeners[i].origin.z = listeners[i].origin.z();
    }

    for (auto i = 0; i < numSources; ++i)
    {
        _directivities[i].dipoleWeight = directivities[i].dipoleWeight;
        _directivities[i].dipolePower = directivities[i].dipolePower;
    }

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mSources.buffer(), _sources,
                            0, nullptr, nullptr);

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mListeners.buffer(), _listeners,
                            0, nullptr, nullptr);

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mDirectivities.buffer(), _directivities,
                            0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::setNumRays(int numSources,
                                               int numListeners,
                                               int numRays)
{
    auto* _numRays = reinterpret_cast<cl_int*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                               mNumRays.clBuffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                               0, mNumRays.size(), 0, nullptr, nullptr, nullptr));

    auto* _numShadowRays = reinterpret_cast<cl_int*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                                     mNumShadowRays.clBuffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION,
                                                     0, mNumShadowRays.size(), 0, nullptr, nullptr, nullptr));

    *_numRays = numListeners * numRays;
    *_numShadowRays = std::max(numSources, numListeners) * numRays;

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mNumRays.clBuffer(), _numRays,
                            0, nullptr, nullptr);

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mNumShadowRays.clBuffer(), _numShadowRays,
                            0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::generateCameraRays(int numRays)
{
    auto n = static_cast<size_t>(floorf(sqrtf(static_cast<float>(numRays))));

    auto argIndex = 0;
    clSetKernelArg(mGenerateCameraRays.kernel(), argIndex++, sizeof(cl_mem), &mListeners.buffer());
    clSetKernelArg(mGenerateCameraRays.kernel(), argIndex++, sizeof(cl_mem), &mRays[mCurrentRayBuffer].clBuffer());

    size_t globalSize[] = { n, n };
    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mGenerateCameraRays.kernel(), 2, nullptr,
                           globalSize, nullptr, 0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::generateListenerRays(int numListeners,
                                                         int numRays)
{
    auto argIndex = 0;
    clSetKernelArg(mGenerateListenerRays.kernel(), argIndex++, sizeof(cl_mem), &mListeners.buffer());
    clSetKernelArg(mGenerateListenerRays.kernel(), argIndex++, sizeof(cl_mem), &mListenerSamples.buffer());
    clSetKernelArg(mGenerateListenerRays.kernel(), argIndex++, sizeof(cl_mem), &mRays[mCurrentRayBuffer].clBuffer());

    size_t globalSize[] = { static_cast<size_t>(numRays), static_cast<size_t>(numListeners) };
    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mGenerateListenerRays.kernel(), 2, nullptr,
                           globalSize, nullptr, 0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::sphereOcclusion(int numSources,
                                                    const CoordinateSpace3f* sources,
                                                    int numListeners,
                                                    const CoordinateSpace3f* listeners,
                                                    int numRays)
{
    auto _numSources = static_cast<cl_uint>(numSources);
    auto _numListeners = static_cast<cl_uint>(numListeners);

    auto argIndex = 0;
    clSetKernelArg(mSphereOcclusion.kernel(), argIndex++, sizeof(cl_uint), &_numSources);
    clSetKernelArg(mSphereOcclusion.kernel(), argIndex++, sizeof(cl_mem), &mSources.buffer());
    clSetKernelArg(mSphereOcclusion.kernel(), argIndex++, sizeof(cl_uint), &_numListeners);
    clSetKernelArg(mSphereOcclusion.kernel(), argIndex++, sizeof(cl_mem), &mListeners.buffer());
    clSetKernelArg(mSphereOcclusion.kernel(), argIndex++, sizeof(cl_mem), &mRays[mCurrentRayBuffer].clBuffer());
    clSetKernelArg(mSphereOcclusion.kernel(), argIndex++, sizeof(cl_mem), &mHits.clBuffer());

    size_t globalSize[] = { static_cast<size_t>(numRays) };

    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mSphereOcclusion.kernel(), 1, nullptr,
                           globalSize, nullptr, 0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::shadeAndBounce(const RadeonRaysScene& scene,
                                                   int numSources,
                                                   const CoordinateSpace3f* sources,
                                                   int numListeners,
                                                   const CoordinateSpace3f* listeners,
                                                   const Directivity* directivities,
                                                   int numRays,
                                                   int numBounces,
                                                   float irradianceMinDistance,
                                                   float scalar)
{
    cl_uint _numSources = static_cast<cl_uint>(numSources);
    cl_uint _numListeners = static_cast<cl_uint>(numListeners);
    cl_uint _numRays = static_cast<cl_uint>(numRays);
    cl_uint _numBounces = static_cast<cl_uint>(numBounces);
    cl_uint _numDiffuseSamples = static_cast<cl_uint>(mNumDiffuseSamples);
    cl_uint _randomNumber = static_cast<cl_uint>(mRNG.uniformRandom());

    const auto* staticMesh = static_cast<const RadeonRaysStaticMesh*>(scene.staticMeshes().front().get());

    auto argIndex = 0;
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_uint), &_numSources);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mSources.buffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_uint), &_numListeners);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mListeners.buffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mDirectivities.buffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_uint), &_numRays);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_uint), &_numBounces);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_float), &irradianceMinDistance);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mRays[mCurrentRayBuffer].clBuffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mHits.clBuffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &staticMesh->normals());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &staticMesh->materialIndices());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &staticMesh->materials());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_uint), &_numDiffuseSamples);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mDiffuseSamples.buffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_uint), &_randomNumber);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_float), &scalar);
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mShadowRays.clBuffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mRays[1 - mCurrentRayBuffer].clBuffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mEnergy.buffer());
    clSetKernelArg(mShadeAndBounce.kernel(), argIndex++, sizeof(cl_mem), &mAccumEnergy.buffer());

    size_t globalSize[] = { static_cast<size_t>(numRays * std::max(numSources, numListeners)) };

    auto status = CL_SUCCESS;
    do
    {
        size_t localSize[] = { static_cast<size_t>(mShadeLocalSize) };
        status = clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mShadeAndBounce.kernel(), 1, nullptr,
                                        globalSize, localSize, 0, nullptr, nullptr);

        if (status != CL_SUCCESS)
        {
            mShadeLocalSize /= 2;
        }
    }
    while (mShadeLocalSize > 1 && status != CL_SUCCESS);
}

void RadeonRaysReflectionSimulator::gatherImage(int numSources,
                                                int numRays)
{
    auto _numSources = static_cast<cl_uint>(numSources);

    auto argIndex = 0;
    clSetKernelArg(mGatherImage.kernel(), argIndex++, sizeof(cl_uint), &_numSources);
    clSetKernelArg(mGatherImage.kernel(), argIndex++, sizeof(cl_mem), &mOccluded.clBuffer());
    clSetKernelArg(mGatherImage.kernel(), argIndex++, sizeof(cl_mem), &mEnergy.buffer());
    clSetKernelArg(mGatherImage.kernel(), argIndex++, sizeof(cl_mem), &mImage.buffer());

    size_t globalSize[] = { static_cast<size_t>(numRays) };
    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mGatherImage.kernel(), 1, nullptr,
                           globalSize, nullptr, 0, nullptr, nullptr);
}

void RadeonRaysReflectionSimulator::gatherEnergyField(int index,
                                                      int numRays,
                                                      OpenCLEnergyField& energyField)
{
    auto scale = kHistogramScale;
    auto rayDiv = static_cast<cl_uint>(std::max(256, numRays));
    auto offset = static_cast<cl_uint>(index * numRays);

    auto argIndex = 0;
    clSetKernelArg(mGatherEnergyField.kernel(), argIndex++, sizeof(cl_float), &scale);
    clSetKernelArg(mGatherEnergyField.kernel(), argIndex++, sizeof(cl_mem), &mEnergy.buffer());
    clSetKernelArg(mGatherEnergyField.kernel(), argIndex++, sizeof(cl_uint), &offset);
    clSetKernelArg(mGatherEnergyField.kernel(), argIndex++, sizeof(cl_mem), &mOccluded.clBuffer());
    clSetKernelArg(mGatherEnergyField.kernel(), argIndex++, sizeof(cl_mem), &mListenerCoeffs.buffer());
    clSetKernelArg(mGatherEnergyField.kernel(), argIndex++, sizeof(cl_mem), &energyField.buffer());

    size_t globalSize[] = { static_cast<size_t>(rayDiv), static_cast<size_t>(Bands::kNumBands), static_cast<size_t>(energyField.numChannels()) };
    size_t localSize[] = { 256, 1, 1 };
    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mGatherEnergyField.kernel(), 3, nullptr,
                           globalSize, localSize, 0, nullptr, nullptr);
}

}

#endif
