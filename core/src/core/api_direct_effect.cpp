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

#include "direct_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_direct_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CDirectEffect
// --------------------------------------------------------------------------------------------------------------------

IPLint32 CDirectEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CDirectEffect::getTail(IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}

CDirectEffect::CDirectEffect(CContext* context,
                             IPLAudioSettings* audioSettings,
                             IPLDirectEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    AudioSettings _audioSettings{};
    _audioSettings.samplingRate = audioSettings->samplingRate;
    _audioSettings.frameSize = audioSettings->frameSize;

    DirectEffectSettings _effectSettings{};
    _effectSettings.numChannels = effectSettings->numChannels;

    new (&mHandle) Handle<DirectEffect>(ipl::make_shared<DirectEffect>(_audioSettings, _effectSettings), _context);
}

IDirectEffect* CDirectEffect::retain()
{
    mHandle.retain();
    return this;
}

void CDirectEffect::release()
{
    if (mHandle.release())
    {
        this->~CDirectEffect();
        gMemory().free(this);
    }
}

void CDirectEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CDirectEffect::apply(IPLDirectEffectParams* params,
                                         IPLAudioBuffer* in,
                                         IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    DirectEffectParams _params{};

    _params.directPath.distanceAttenuation = params->distanceAttenuation;
    _params.directPath.airAbsorption[0] = params->airAbsorption[0];
    _params.directPath.airAbsorption[1] = params->airAbsorption[1];
    _params.directPath.airAbsorption[2] = params->airAbsorption[2];
    _params.directPath.directivity = params->directivity;
    _params.directPath.occlusion = params->occlusion;
    _params.directPath.transmission[0] = params->transmission[0];
    _params.directPath.transmission[1] = params->transmission[1];
    _params.directPath.transmission[2] = params->transmission[2];

    _params.flags = static_cast<DirectEffectFlags>(params->flags);
    _params.transmissionType = static_cast<TransmissionType>(params->transmissionType);

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createDirectEffect(IPLAudioSettings* audioSettings,
                                      IPLDirectEffectSettings* effectSettings,
                                      IDirectEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CDirectEffect*>(gMemory().allocate(sizeof(CDirectEffect), Memory::kDefaultAlignment));
        new (_effect) CDirectEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
