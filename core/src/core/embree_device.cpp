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

#include <stdint.h>

#include "embree_device.h"

#include "log.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeDevice
// --------------------------------------------------------------------------------------------------------------------

EmbreeDevice::EmbreeDevice()
{
    mDevice = rtcNewDevice(nullptr);

    int64_t major = rtcDeviceGetParameter1i(mDevice, RTC_CONFIG_VERSION_MAJOR);
    int64_t minor = rtcDeviceGetParameter1i(mDevice, RTC_CONFIG_VERSION_MINOR);
    int64_t patch = rtcDeviceGetParameter1i(mDevice, RTC_CONFIG_VERSION_PATCH);

    gLog().message(MessageSeverity::Info, "Initialized Embree v%lld.%02lld.%02lld.", major, minor, patch);
}

EmbreeDevice::~EmbreeDevice()
{
    rtcDeleteDevice(mDevice);
}

}

#endif
