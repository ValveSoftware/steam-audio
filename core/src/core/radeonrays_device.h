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

#define USE_OPENCL 1
#define RR_STATIC_LIBRARY 1
#include <radeon_rays_cl.h>

#include "opencl_buffer.h"
#include "opencl_device.h"
#include "opencl_kernel.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysDevice
// --------------------------------------------------------------------------------------------------------------------

class RadeonRaysDevice
{
public:
    RadeonRaysDevice(shared_ptr<OpenCLDevice> openCL);

    ~RadeonRaysDevice();

    const OpenCLDevice& openCL() const
    {
        return *mOpenCL;
    }

    RadeonRays::IntersectionApi* api() const
    {
        return mAPI;
    }

    const OpenCLProgram& program() const
    {
        return mProgram;
    }

private:
    shared_ptr<OpenCLDevice> mOpenCL;
    RadeonRays::IntersectionApi* mAPI;
    OpenCLProgram mProgram;
};


// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysBuffer
// --------------------------------------------------------------------------------------------------------------------

class RadeonRaysBuffer
{
public:
    RadeonRaysBuffer(shared_ptr<RadeonRaysDevice> radeonRays,
                     size_t size);

    ~RadeonRaysBuffer();

    size_t size() const
    {
        return mCLBuffer.size();
    }

    cl_mem& clBuffer()
    {
        return mCLBuffer.buffer();
    }

    RadeonRays::Buffer* rrBuffer()
    {
        return mRRBuffer;
    }

private:
    shared_ptr<RadeonRaysDevice> mRadeonRays;
    OpenCLBuffer mCLBuffer;
    RadeonRays::Buffer* mRRBuffer;
};

}

#else

namespace ipl {

class RadeonRaysDevice
{};

}

#endif