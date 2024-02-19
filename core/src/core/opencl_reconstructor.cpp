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

#include "opencl_reconstructor.h"

#include "sh.h"
#include "radeonrays_reflection_simulator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLReconstructor
// --------------------------------------------------------------------------------------------------------------------

OpenCLReconstructor::OpenCLReconstructor(shared_ptr<RadeonRaysDevice> radeonRays,
                                         float maxDuration,
                                         int maxOrder,
                                         int samplingRate)
    : mRadeonRays(radeonRays)
    , mNumChannels(SphericalHarmonics::numCoeffsForOrder(maxOrder))
    , mNumSamples(static_cast<int>(ceilf(maxDuration * samplingRate)))
    , mSamplingRate(samplingRate)
    , mAirAbsorption(radeonRays->openCL(), Bands::kNumBands * sizeof(cl_float))
    , mBandFilters(radeonRays->openCL(), Bands::kNumBands * sizeof(IIR))
    , mWhiteNoise(radeonRays->openCL(), mNumChannels * Bands::kNumBands * mNumSamples * sizeof(cl_float))
    , mBatchedBandIRs(radeonRays->openCL(), kBatchSize * mNumChannels * Bands::kNumBands * mNumSamples * sizeof(cl_float))
    , mBatchedIR(radeonRays->openCL(), kBatchSize * mNumChannels * mNumSamples * sizeof(cl_float))
    , mReconstruct(radeonRays->openCL(), radeonRays->program(), "reconstructImpulseResponse")
    , mApplyIIR(radeonRays->openCL(), radeonRays->program(), "applyIIRFilter")
    , mCombine(radeonRays->openCL(), radeonRays->program(), "combineBandpassedImpulseResponse")
{
    auto status = CL_SUCCESS;
    auto* whiteNoise = reinterpret_cast<float*>(clEnqueueMapBuffer(radeonRays->openCL().irUpdateQueue(), mWhiteNoise.buffer(),
                                                CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, mWhiteNoise.size(),
                                                0, nullptr, nullptr, &status));
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    for (auto band = 0, i = 0; band < Bands::kNumBands; ++band)
    {
        std::default_random_engine randomGenerator;
        std::uniform_real_distribution<float> uniformDistribution(-1.0f, 1.0f);

        auto uniformRandom = std::bind(uniformDistribution, randomGenerator);

        for (auto sample = 0; sample < mNumSamples; ++sample, ++i)
        {
            auto value = uniformRandom();

            for (auto j = 0; j < mNumChannels; ++j)
            {
                whiteNoise[j * Bands::kNumBands * mNumSamples + i] = value;
            }
        }
    }

    status = clEnqueueUnmapMemObject(radeonRays->openCL().irUpdateQueue(), mWhiteNoise.buffer(), whiteNoise,
                                     0, nullptr, nullptr);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    IIR filters[Bands::kNumBands];
    filters[0] = IIR::lowPass(Bands::kHighCutoffFrequencies[0], samplingRate);

    for (auto i = 1; i < Bands::kNumBands - 1; ++i)
    {
        filters[i] = IIR::bandPass(Bands::kLowCutoffFrequencies[i], Bands::kHighCutoffFrequencies[i], samplingRate);
    }

    filters[Bands::kNumBands - 1] = IIR::highPass(Bands::kLowCutoffFrequencies[Bands::kNumBands - 1], samplingRate);

    status = clEnqueueWriteBuffer(radeonRays->openCL().irUpdateQueue(), mBandFilters.buffer(), CL_TRUE, 0,
                                  mBandFilters.size(), filters, 0, nullptr, nullptr);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    AirAbsorptionModel airAbsorption;
    status = clEnqueueWriteBuffer(radeonRays->openCL().irUpdateQueue(), mAirAbsorption.buffer(), CL_TRUE, 0,
                                  mAirAbsorption.size(), airAbsorption.coefficients, 0, nullptr, nullptr);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);
}

// todo: apply distance attenuation correction curves
// todo: apply air absorption models (non-default)
// todo: linear reconstruction
void OpenCLReconstructor::reconstruct(int numIRs,
                                      const EnergyField* const* energyFields,
                                      const float* const* distanceAttenuationCorrectionCurves,
                                      const AirAbsorptionModel* airAbsorptionModels,
                                      ImpulseResponse* const* impulseResponses,
                                      ReconstructionType type,
                                      float duration,
                                      int order)
{
    for (auto i = 0; i < numIRs; i += kBatchSize)
    {
        auto batchSize = std::min(kBatchSize, numIRs - i);

        auto numBins = std::numeric_limits<int>::max();
        for (auto j = 0; j < batchSize; ++j)
        {
            numBins = std::min(numBins, energyFields[i + j]->numBins());
            reconstruct(*static_cast<const OpenCLEnergyField*>(energyFields[i + j]), j);
        }

        applyIIR(numBins, batchSize);
        combine(numBins, batchSize);

        for (auto j = 0; j < batchSize; ++j)
        {
            auto& ir = *static_cast<OpenCLImpulseResponse*>(impulseResponses[i + j]);
            auto size = mNumChannels * mNumSamples * sizeof(float);
            auto channelSize = mNumSamples * sizeof(float);

            for (auto k = 0; k < mNumChannels; ++k)
            {
                clEnqueueCopyBuffer(mRadeonRays->openCL().irUpdateQueue(), mBatchedIR.buffer(), ir.channelBuffers()[k],
                                    j * size + k * channelSize, 0, channelSize, 0, nullptr, nullptr);
            }
        }
    }

    clFlush(mRadeonRays->openCL().irUpdateQueue());
}

void OpenCLReconstructor::reconstruct(const OpenCLEnergyField& energyField,
                                      int index)
{
    auto _samplingRate = static_cast<cl_uint>(mSamplingRate);
    auto _samplesPerBin = static_cast<cl_uint>(mNumSamples / energyField.numBins());
    auto _numSamples = static_cast<cl_uint>(mNumSamples);
    auto _offset = index * mNumChannels * Bands::kNumBands * mNumSamples;
    auto _scale = RadeonRaysReflectionSimulator::kHistogramScale;

    auto argIndex = 0;
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_mem), &energyField.buffer());
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_uint), &_samplingRate);
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_uint), &_samplesPerBin);
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_uint), &_numSamples);
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_mem), &mAirAbsorption.buffer());
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_mem), &mBandFilters.buffer());
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_mem), &mWhiteNoise.buffer());
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_mem), &mBatchedBandIRs.buffer());
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_uint), &_offset);
    clSetKernelArg(mReconstruct.kernel(), argIndex++, sizeof(cl_float), &_scale);

    size_t globalSize[] = { static_cast<size_t>(energyField.numBins()), static_cast<size_t>(Bands::kNumBands), static_cast<size_t>(energyField.numChannels()) };
    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mReconstruct.kernel(), 3, nullptr,
                           globalSize, nullptr, 0, nullptr, nullptr);
}

void OpenCLReconstructor::applyIIR(int numBins,
                                   int batchSize)
{
    auto _numBins = static_cast<cl_uint>(numBins);
    auto _samplesPerBin = static_cast<cl_uint>(mNumSamples / numBins);
    auto _numSamples = static_cast<cl_uint>(mNumSamples);

    auto argIndex = 0;
    clSetKernelArg(mApplyIIR.kernel(), argIndex++, sizeof(cl_mem), &mBandFilters.buffer());
    clSetKernelArg(mApplyIIR.kernel(), argIndex++, sizeof(cl_mem), &mBatchedBandIRs.buffer());
    clSetKernelArg(mApplyIIR.kernel(), argIndex++, sizeof(cl_uint), &_numBins);
    clSetKernelArg(mApplyIIR.kernel(), argIndex++, sizeof(cl_uint), &_samplesPerBin);
    clSetKernelArg(mApplyIIR.kernel(), argIndex++, sizeof(cl_uint), &_numSamples);

    size_t globalSize[] = { static_cast<size_t>(Bands::kNumBands), static_cast<size_t>(mNumChannels), static_cast<size_t>(batchSize) };
    size_t localSize[] = { static_cast<size_t>(std::min(Bands::kNumBands, 8)), (mNumChannels == 16) ? 8 : static_cast<size_t>(mNumChannels), 1u };
    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mApplyIIR.kernel(), 3, nullptr,
                           globalSize, localSize, 0, nullptr, nullptr);
}

void OpenCLReconstructor::combine(int numBins,
                                  int batchSize)
{
    auto _numSamples = static_cast<cl_uint>(mNumSamples);
    auto _samplesPerBin = static_cast<cl_uint>(mNumSamples / numBins);

    auto argIndex = 0;
    clSetKernelArg(mCombine.kernel(), argIndex++, sizeof(cl_uint), &_numSamples);
    clSetKernelArg(mCombine.kernel(), argIndex++, sizeof(cl_mem), &mBatchedBandIRs.buffer());
    clSetKernelArg(mCombine.kernel(), argIndex++, sizeof(cl_mem), &mBatchedIR.buffer());

    size_t globalSize[] = { static_cast<size_t>(numBins * _samplesPerBin), static_cast<size_t>(mNumChannels), static_cast<size_t>(batchSize) };
    clEnqueueNDRangeKernel(mRadeonRays->openCL().irUpdateQueue(), mCombine.kernel(), 3, nullptr,
                           globalSize, nullptr, 0, nullptr, nullptr);
}

}

#endif
