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

#include "radeonrays_device.h"

#include "log.h"
#include "radeonrays_reflection_simulator.cl.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysDevice
// --------------------------------------------------------------------------------------------------------------------

RadeonRaysDevice::RadeonRaysDevice(shared_ptr<OpenCLDevice> openCL)
    : mOpenCL(openCL)
    , mProgram(*openCL, gKernelSource)
{
    mAPI = RadeonRays::CreateFromOpenClContext(openCL->context(), openCL->device(), openCL->irUpdateQueue());

    mAPI->SetOption("bvh.builder", "sah");
    mAPI->SetOption("bvh.usesplits", "1");

    gLog().message(MessageSeverity::Info, "Initialized Radeon Rays v%.2f.", RADEONRAYS_API_VERSION);
}

RadeonRaysDevice::~RadeonRaysDevice()
{
    RadeonRays::IntersectionApi::Delete(mAPI);
}


// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysBuffer
// --------------------------------------------------------------------------------------------------------------------

RadeonRaysBuffer::RadeonRaysBuffer(shared_ptr<RadeonRaysDevice> radeonRays,
                                   size_t size)
    : mRadeonRays(radeonRays)
    , mCLBuffer(radeonRays->openCL(), size)
{
    mRRBuffer = RadeonRays::CreateFromOpenClBuffer(radeonRays->api(), mCLBuffer.buffer());
}

RadeonRaysBuffer::~RadeonRaysBuffer()
{
    mRadeonRays->api()->DeleteBuffer(mRRBuffer);
}

}

#endif