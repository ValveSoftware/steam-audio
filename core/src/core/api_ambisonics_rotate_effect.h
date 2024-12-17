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

#include "ambisonics_rotate_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CAmbisonicsRotationEffect
// --------------------------------------------------------------------------------------------------------------------

class CAmbisonicsRotationEffect : public IAmbisonicsRotationEffect
{
public:
    Handle<AmbisonicsRotateEffect> mHandle;

    CAmbisonicsRotationEffect(CContext* context,
                              IPLAudioSettings* audioSettings,
                              IPLAmbisonicsRotationEffectSettings* effectSettings);

    virtual IAmbisonicsRotationEffect* retain() override;

    virtual void release() override;

    virtual void reset() override;

    virtual IPLAudioEffectState apply(IPLAmbisonicsRotationEffectParams* params,
                                      IPLAudioBuffer* in,
                                      IPLAudioBuffer* out) override;

    virtual IPLint32 getTailSize() override;

    virtual IPLAudioEffectState getTail(IPLAudioBuffer* out) override;
};

}
