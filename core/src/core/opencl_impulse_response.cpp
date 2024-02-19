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

#if defined(IPL_USES_OPENCL)

#include "opencl_impulse_response.h"

#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLImpulseResponse
// --------------------------------------------------------------------------------------------------------------------

OpenCLImpulseResponse::OpenCLImpulseResponse(shared_ptr<OpenCLDevice> openCL,
                                             float duration,
                                             int order,
                                             int samplingRate)
    : ImpulseResponse(duration, order, samplingRate)
    , mOpenCL(openCL)
    , mSize(numSamples() * sizeof(cl_float))
    , mPaddedSize(openCL->paddedSize(mSize))
    , mBuffer(*openCL, numChannels() * mPaddedSize)
    , mChannelBuffers(numChannels())
{
    auto status = CL_SUCCESS;

    cl_buffer_region region;
    region.origin = 0;
    region.size = mSize;

    for (auto i = 0; i < numChannels(); ++i)
    {
        mChannelBuffers[i] = clCreateSubBuffer(mBuffer.buffer(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                               &region, &status);
        if (status != CL_SUCCESS)
            throw Exception(Status::Initialization);

        region.origin += mPaddedSize;
    }
}

OpenCLImpulseResponse::~OpenCLImpulseResponse()
{
    for (auto i = 0; i < numChannels(); ++i)
    {
        clReleaseMemObject(mChannelBuffers[i]);
    }
}

void OpenCLImpulseResponse::reset()
{
    ImpulseResponse::reset();

    auto zero = 0.0f;
    clEnqueueFillBuffer(mOpenCL->irUpdateQueue(), mBuffer.buffer(), &zero, sizeof(cl_float), 0, mBuffer.size(),
                        0, nullptr, nullptr);
}

void OpenCLImpulseResponse::copyDeviceToHost()
{
    for (auto i = 0; i < numChannels(); ++i)
    {
        clEnqueueReadBuffer(mOpenCL->irUpdateQueue(), mChannelBuffers[i], CL_TRUE, 0, mSize,
                            &mData.flatData()[i * numSamples()], 0, nullptr, nullptr);
    }
}

void OpenCLImpulseResponse::copyHostToDevice()
{
    for (auto i = 0; i < numChannels(); ++i)
    {
        clEnqueueWriteBuffer(mOpenCL->irUpdateQueue(), mChannelBuffers[i], CL_TRUE, 0, mSize,
                             &mData.flatData()[i * numSamples()], 0, nullptr, nullptr);
    }
}

}

#endif
