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

#include "tan_device.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_opencl_device.h"
#include "api_tan_device.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CTrueAudioNextDevice
// --------------------------------------------------------------------------------------------------------------------

CTrueAudioNextDevice::CTrueAudioNextDevice(COpenCLDevice* openCLDevice,
                                           IPLTrueAudioNextDeviceSettings* settings)
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    auto _context = openCLDevice->mHandle.context();
    if (!_context)
        throw Exception(Status::Failure);

    auto _openCLDevice = openCLDevice->mHandle.get();
    if (!_openCLDevice)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<TANDevice>(ipl::make_shared<TANDevice>(
        _openCLDevice->convolutionQueue(), _openCLDevice->irUpdateQueue(),
        settings->frameSize, settings->irSize, settings->order, settings->maxSources), _context);
#endif
}

ITrueAudioNextDevice* CTrueAudioNextDevice::retain()
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    mHandle.retain();
    return this;
#else
    return nullptr;
#endif
}

void CTrueAudioNextDevice::release()
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    if (mHandle.release())
    {
        this->~CTrueAudioNextDevice();
        gMemory().free(this);
    }
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// COpenCLDevice
// --------------------------------------------------------------------------------------------------------------------

IPLerror COpenCLDevice::createTrueAudioNextDevice(IPLTrueAudioNextDeviceSettings* settings,
                                                  ITrueAudioNextDevice** device)
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    if (!settings || !device)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _device = reinterpret_cast<CTrueAudioNextDevice*>(gMemory().allocate(sizeof(CTrueAudioNextDevice), Memory::kDefaultAlignment));
        new (_device) CTrueAudioNextDevice(this, settings);
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
