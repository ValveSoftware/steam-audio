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

#include "error.h"
#include "context.h"
#include "profiler.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CHRTF
// --------------------------------------------------------------------------------------------------------------------

class CHRTF : public IHRTF
{
public:
    Handle<HRTFDatabase> mHandle;

    CHRTF(CContext* context,
          IPLAudioSettings* audioSettings,
          IPLHRTFSettings* hrtfSettings);

    virtual IHRTF* retain() override;

    virtual void release() override;
};

}