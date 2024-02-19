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

#include "radeonrays_device.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_opencl_device.h"
#include "api_radeonrays_device.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CRadeonRaysDevice
// --------------------------------------------------------------------------------------------------------------------

CRadeonRaysDevice::CRadeonRaysDevice(COpenCLDevice* openCLDevice,
                                     IPLRadeonRaysDeviceSettings* settings)
{
#if defined(IPL_USES_RADEONRAYS)
    auto _context = openCLDevice->mHandle.context();
    if (!_context)
        throw Exception(Status::Failure);

    auto _openCLDevice = openCLDevice->mHandle.get();
    if (!_openCLDevice)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<RadeonRaysDevice>(ipl::make_shared<ipl::RadeonRaysDevice>(_openCLDevice), _context);
#endif
}

IRadeonRaysDevice* CRadeonRaysDevice::retain()
{
#if defined(IPL_USES_RADEONRAYS)
    mHandle.retain();
    return this;
#else
    return nullptr;
#endif
}

void CRadeonRaysDevice::release()
{
#if defined(IPL_USES_RADEONRAYS)
    if (mHandle.release())
    {
        this->~CRadeonRaysDevice();
        gMemory().free(this);
    }
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// COpenCLDevice
// --------------------------------------------------------------------------------------------------------------------

IPLerror COpenCLDevice::createRadeonRaysDevice(IPLRadeonRaysDeviceSettings* settings,
                                               IRadeonRaysDevice** device)
{
#if defined(IPL_USES_RADEONRAYS)
    if (!device)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _device = reinterpret_cast<CRadeonRaysDevice*>(gMemory().allocate(sizeof(CRadeonRaysDevice), Memory::kDefaultAlignment));
        new (_device) CRadeonRaysDevice(this, settings);
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
