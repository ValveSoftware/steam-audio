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
#include "api_ambisonics_rotate_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CAmbisonicsRotationEffect
// --------------------------------------------------------------------------------------------------------------------

IPLint32 CAmbisonicsRotationEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CAmbisonicsRotationEffect::getTail(IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}

CAmbisonicsRotationEffect::CAmbisonicsRotationEffect(CContext* context,
                                                     IPLAudioSettings* audioSettings,
                                                     IPLAmbisonicsRotationEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    AudioSettings _audioSettings{};
    _audioSettings.samplingRate = audioSettings->samplingRate;
    _audioSettings.frameSize = audioSettings->frameSize;

    AmbisonicsRotateEffectSettings _effectSettings{};
    _effectSettings.maxOrder = effectSettings->maxOrder;

    new (&mHandle) Handle<AmbisonicsRotateEffect>(ipl::make_shared<AmbisonicsRotateEffect>(_audioSettings, _effectSettings), _context);
}

IAmbisonicsRotationEffect* CAmbisonicsRotationEffect::retain()
{
    mHandle.retain();
    return this;
}

void CAmbisonicsRotationEffect::release()
{
    if (mHandle.release())
    {
        this->~CAmbisonicsRotationEffect();
        gMemory().free(this);
    }
}

void CAmbisonicsRotationEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CAmbisonicsRotationEffect::apply(IPLAmbisonicsRotationEffectParams* params,
                                                     IPLAudioBuffer* in,
                                                     IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    AmbisonicsRotateEffectParams _params{};
    _params.orientation = reinterpret_cast<const CoordinateSpace3f*>(&params->orientation);
    _params.order = params->order;

    return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createAmbisonicsRotationEffect(IPLAudioSettings* audioSettings,
                                                  IPLAmbisonicsRotationEffectSettings* effectSettings,
                                                  IAmbisonicsRotationEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CAmbisonicsRotationEffect*>(gMemory().allocate(sizeof(CAmbisonicsRotationEffect), Memory::kDefaultAlignment));
        new (_effect) CAmbisonicsRotationEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
