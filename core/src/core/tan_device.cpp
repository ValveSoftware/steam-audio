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

#if defined(IPL_USES_TRUEAUDIONEXT)

#include "tan_device.h"

#include "array_math.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// TANDevice
// --------------------------------------------------------------------------------------------------------------------

TANDevice::TANDevice(cl_command_queue convolutionQueue,
                     cl_command_queue updateQueue,
                     int frameSize,
                     int irSize,
                     int order,
                     int numSources)
    : mFrameSize(frameSize)
    , mIRSize(irSize)
    , mNumChannels(SphericalHarmonics::numCoeffsForOrder(order))
    , mNumSources(numSources)
    , mDry(numSources, frameSize)
    , mDryPtrs(numSources, mNumChannels)
    , mWet(numSources, mNumChannels, frameSize)
    , mWetPtrs(numSources, mNumChannels)
    , mIR(numSources, mNumChannels)
    , mUpdateFlags(numSources, mNumChannels)
    , mProcessFlags(numSources, mNumChannels)
    , mIRsUpdated(false)
{
    for (auto i = 0; i < numSources; ++i)
    {
        mSlots.push(i);
    }

    auto status = TANCreateContext(TAN_FULL_VERSION, &mTANContext);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    status = mTANContext->InitOpenCL(updateQueue, convolutionQueue);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    status = TANCreateConvolution(mTANContext, &mTANConvolution);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    status = mTANConvolution->InitGpu(amf::TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED, mIRSize, mFrameSize, mNumSources * mNumChannels);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    mDry.zero();
    mWet.zero();

    for (auto i = 0; i < numSources; ++i)
    {
        for (auto j = 0; j < mNumChannels; ++j)
        {
            mDryPtrs[i][j] = mDry[i];
            mWetPtrs[i][j] = mWet[i][j];
            mUpdateFlags[i][j] = amf::TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT;
            mProcessFlags[i][j] = amf::TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT;
        }
    }
}

int TANDevice::acquireSlot()
{
    if (mSlots.empty())
        return -1;

    auto slot = mSlots.top();
    mSlots.pop();

    return slot;
}

void TANDevice::releaseSlot(int slot)
{
    mSlots.push(slot);
}

void TANDevice::reset(int slot)
{
    for (auto i = 0; i < mNumChannels; ++i)
    {
        mProcessFlags[slot][i] = amf::TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT | amf::TAN_CONVOLUTION_CHANNEL_FLAG_FLUSH_STREAM;
    }
}

void TANDevice::setDry(int slot,
                       const AudioBuffer& in)
{
    memcpy(mDry[slot], in[0], mFrameSize * sizeof(float));

    for (auto i = 0; i < mNumChannels; ++i)
    {
        mProcessFlags[slot][i] = amf::TAN_CONVOLUTION_CHANNEL_FLAG_PROCESS;
    }
}

void TANDevice::process(AudioBuffer& out)
{
    out.makeSilent();

    if (!mIRsUpdated)
        return;

    mWet.zero();

    mTANConvolution->Process(mDryPtrs.flatData(), mWetPtrs.flatData(), mFrameSize, mProcessFlags.flatData(), nullptr);

    for (auto i = 0; i < mNumSources; ++i)
    {
        if (mProcessFlags[i][0] == amf::TAN_CONVOLUTION_CHANNEL_FLAG_PROCESS)
        {
            for (auto j = 0; j < mNumChannels; ++j)
            {
                ArrayMath::add(mFrameSize, mWet[i][j], out[j], out[j]);
            }
        }
    }

    for (auto i = 0; i < mNumSources; ++i)
    {
        for (auto j = 0; j < mNumChannels; ++j)
        {
            mProcessFlags[i][j] = amf::TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT;
        }
    }
}

void TANDevice::setIR(int slot,
                      const cl_mem* irChannels)
{
    for (auto i = 0; i < mNumChannels; ++i)
    {
        mIR[slot][i] = irChannels[i];
        mUpdateFlags[slot][i] = amf::TAN_CONVOLUTION_CHANNEL_FLAG_PROCESS;
    }
}

void TANDevice::updateIRs()
{
    auto status = mTANConvolution->UpdateResponseTD(mIR.flatData(), mIRSize, mUpdateFlags.flatData(),
                                                    amf::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY);

    if (status == AMF_OK)
    {
        mIRsUpdated = true;
    }

    for (auto i = 0; i < mNumSources; ++i)
    {
        for (auto j = 0; j < mNumChannels; ++j)
        {
            mUpdateFlags[i][j] = amf::TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT;
        }
    }
}

}

#endif