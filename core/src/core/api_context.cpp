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

#include "error.h"
#include "context.h"
#include "hrtf_database.h"
#include "path_simulator.h"
#include "profiler.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

bool CContext::isVersionCompatible(IPLuint32 version)
{
    auto major = (version >> 16) & 0xff;
    auto minor = (version >> 8) & 0xff;
    auto patch = version & 0xff;

    return (major == STEAMAUDIO_VERSION_MAJOR && minor <= STEAMAUDIO_VERSION_MINOR);
}

CContext::CContext(IPLContextSettings* settings)
{
    auto _logCallback = reinterpret_cast<LogCallback>(settings->logCallback);
    auto _allocateCallback = reinterpret_cast<AllocateCallback>(settings->allocateCallback);
    auto _freeCallback = reinterpret_cast<FreeCallback>(settings->freeCallback);
    auto _simdLevel = static_cast<SIMDLevel>(settings->simdLevel);

    new (&mHandle) Handle<Context>(ipl::make_shared<Context>(_logCallback, _allocateCallback, _freeCallback, _simdLevel, settings->version), nullptr);
}

IContext* CContext::retain()
{
    mHandle.retain();
    return this;
}

void CContext::release()
{
    if (mHandle.release())
    {
        this->~CContext();
        gMemory().free(this);
    }
}

void CContext::setProfilerContext(void* profilerContext)
{
    Profiler::setProfilerContext(profilerContext);
}

}


// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

#if defined(STEAMAUDIO_BUILDING_CORE)
IPLerror IPLCALL iplContextCreate(IPLContextSettings* settings,
                          IPLContext* context)
{
    return api::CContext::createContext(settings, reinterpret_cast<api::IContext**>(context));
}
#endif
