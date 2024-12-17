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

#include "ambisonics_encode_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_ambisonics_encode_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CAmbisonicsEncodeEffect
// --------------------------------------------------------------------------------------------------------------------

IPLint32 CAmbisonicsEncodeEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CAmbisonicsEncodeEffect::getTail(IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}

CAmbisonicsEncodeEffect::CAmbisonicsEncodeEffect(CContext* context,
                                                 IPLAudioSettings* audioSettings,
                                                 IPLAmbisonicsEncodeEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    AmbisonicsEncodeEffectSettings _effectSettings{};
    _effectSettings.maxOrder = effectSettings->maxOrder;

    new (&mHandle) Handle<AmbisonicsEncodeEffect>(ipl::make_shared<AmbisonicsEncodeEffect>(_effectSettings), _context);
}

IAmbisonicsEncodeEffect* CAmbisonicsEncodeEffect::retain()
{
    mHandle.retain();
    return this;
}

void CAmbisonicsEncodeEffect::release()
{
    if (mHandle.release())
    {
        this->~CAmbisonicsEncodeEffect();
        gMemory().free(this);
    }
}

void CAmbisonicsEncodeEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CAmbisonicsEncodeEffect::apply(IPLAmbisonicsEncodeEffectParams* params,
                                                   IPLAudioBuffer* in,
                                                   IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    AmbisonicsEncodeEffectParams _params{};
    _params.direction = reinterpret_cast<const Vector3f*>(&params->direction);
    _params.order = params->order;

    return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createAmbisonicsEncodeEffect(IPLAudioSettings* audioSettings,
                                                IPLAmbisonicsEncodeEffectSettings* effectSettings,
                                                IAmbisonicsEncodeEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CAmbisonicsEncodeEffect*>(gMemory().allocate(sizeof(CAmbisonicsEncodeEffect), Memory::kDefaultAlignment));
        new (_effect) CAmbisonicsEncodeEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
