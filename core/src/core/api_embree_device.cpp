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

#include "embree_device.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_embree_device.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CEmbreeDevice
// --------------------------------------------------------------------------------------------------------------------

CEmbreeDevice::CEmbreeDevice(CContext* context,
                             IPLEmbreeDeviceSettings* settings)
{
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<EmbreeDevice>(ipl::make_shared<EmbreeDevice>(), _context);
#endif
}

IEmbreeDevice* CEmbreeDevice::retain()
{
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    mHandle.retain();
    return this;
#else
    return nullptr;
#endif
}

void CEmbreeDevice::release()
{
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    if (mHandle.release())
    {
        this->~CEmbreeDevice();
        gMemory().free(this);
    }
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createEmbreeDevice(IPLEmbreeDeviceSettings* settings,
                                      IEmbreeDevice** device)
{
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    if (!device)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _device = reinterpret_cast<CEmbreeDevice*>(gMemory().allocate(sizeof(CEmbreeDevice), Memory::kDefaultAlignment));
        new (_device) CEmbreeDevice(this, settings);
        *device = _device;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
#else
    return IPL_STATUS_FAILURE;
#endif
}

}
