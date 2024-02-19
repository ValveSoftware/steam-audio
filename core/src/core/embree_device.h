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

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#define EMBREE_STATIC_LIB
#include <rtcore.h>
#include <rtcore_ray.h>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeDevice
// --------------------------------------------------------------------------------------------------------------------

class EmbreeDevice
{
public:
    EmbreeDevice();

    ~EmbreeDevice();

    RTCDevice device() const
    {
        return mDevice;
    }

private:
    RTCDevice mDevice;
};

}

#else

namespace ipl {

class EmbreeDevice
{};

}

#endif
