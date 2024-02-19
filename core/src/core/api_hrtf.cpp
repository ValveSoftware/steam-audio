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

#include "hrtf_database.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_hrtf.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CHRTF
// --------------------------------------------------------------------------------------------------------------------

CHRTF::CHRTF(CContext* context,
             IPLAudioSettings* audioSettings,
             IPLHRTFSettings* hrtfSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    HRTFSettings _hrtfSettings{};
    _hrtfSettings.type = static_cast<HRTFMapType>(hrtfSettings->type);
    _hrtfSettings.sofaFileName = hrtfSettings->sofaFileName;

    if (Context::isCallerAPIVersionAtLeast(4, 2))
    {
        _hrtfSettings.sofaData = hrtfSettings->sofaData;
        _hrtfSettings.sofaDataSize = hrtfSettings->sofaDataSize;
        _hrtfSettings.volume = Loudness::gainToDB(hrtfSettings->volume);
    }

    if (Context::isCallerAPIVersionAtLeast(4, 3))
    {
        _hrtfSettings.normType = static_cast<HRTFNormType>(hrtfSettings->normType);
    }

    new (&mHandle) Handle<HRTFDatabase>(ipl::make_shared<HRTFDatabase>(_hrtfSettings, audioSettings->samplingRate, audioSettings->frameSize), _context);
}

IHRTF* CHRTF::retain()
{
    mHandle.retain();
    return this;
}

void CHRTF::release()
{
    if (mHandle.release())
    {
        this->~CHRTF();
        gMemory().free(this);
    }
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createHRTF(IPLAudioSettings* audioSettings,
                              IPLHRTFSettings* hrtfSettings,
                              IHRTF** hrtf)
{
    if (!audioSettings || !hrtfSettings || !hrtf)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    if (hrtfSettings->type == IPL_HRTFTYPE_SOFA && hrtfSettings->sofaFileName == nullptr && hrtfSettings->sofaData == nullptr)
        return IPL_STATUS_FAILURE;

    if (hrtfSettings->type == IPL_HRTFTYPE_SOFA && hrtfSettings->sofaData != nullptr && hrtfSettings->sofaDataSize == 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _hrtf = reinterpret_cast<CHRTF*>(gMemory().allocate(sizeof(CHRTF), Memory::kDefaultAlignment));
        new (_hrtf) CHRTF(this, audioSettings, hrtfSettings);
        *hrtf = _hrtf;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
