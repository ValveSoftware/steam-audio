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

#include "ambisonics_decode_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_hrtf.h"
#include "api_ambisonics_decode_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CAmbisonicsDecodeEffect
// --------------------------------------------------------------------------------------------------------------------

IPLint32 CAmbisonicsDecodeEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CAmbisonicsDecodeEffect::getTail(IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}

CAmbisonicsDecodeEffect::CAmbisonicsDecodeEffect(CContext* context,
                                                 IPLAudioSettings* audioSettings,
                                                 IPLAmbisonicsDecodeEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _hrtf = (effectSettings->hrtf) ? reinterpret_cast<CHRTF*>(effectSettings->hrtf)->mHandle.get() : nullptr;

    AudioSettings _audioSettings{};
    _audioSettings.samplingRate = audioSettings->samplingRate;
    _audioSettings.frameSize = audioSettings->frameSize;

    auto _speakerLayout = SpeakerLayout(static_cast<SpeakerLayoutType>(effectSettings->speakerLayout.type),
                                        effectSettings->speakerLayout.numSpeakers,
                                        reinterpret_cast<Vector3f*>(effectSettings->speakerLayout.speakers));

    AmbisonicsDecodeEffectSettings _effectSettings{};
    _effectSettings.speakerLayout = &_speakerLayout;
    _effectSettings.maxOrder = effectSettings->maxOrder;
    _effectSettings.hrtf = _hrtf.get();

    new (&mHandle) Handle<AmbisonicsDecodeEffect>(ipl::make_shared<AmbisonicsDecodeEffect>(_audioSettings, _effectSettings), _context);
}

IAmbisonicsDecodeEffect* CAmbisonicsDecodeEffect::retain()
{
    mHandle.retain();
    return this;
}

void CAmbisonicsDecodeEffect::release()
{
    if (mHandle.release())
    {
        this->~CAmbisonicsDecodeEffect();
        gMemory().free(this);
    }
}

void CAmbisonicsDecodeEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CAmbisonicsDecodeEffect::apply(IPLAmbisonicsDecodeEffectParams* params,
                                                   IPLAudioBuffer* in,
                                                   IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    auto _hrtf = (params->hrtf) ? reinterpret_cast<CHRTF*>(params->hrtf)->mHandle.get() : nullptr;
    if (params->binaural && !_hrtf)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    AmbisonicsDecodeEffectParams _params{};
    _params.order = params->order;
    _params.orientation = reinterpret_cast<const CoordinateSpace3f*>(&params->orientation);
    _params.binaural = (params->binaural == IPL_TRUE);
    _params.hrtf = _hrtf.get();

    return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createAmbisonicsDecodeEffect(IPLAudioSettings* audioSettings,
                                                IPLAmbisonicsDecodeEffectSettings* effectSettings,
                                                IAmbisonicsDecodeEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CAmbisonicsDecodeEffect*>(gMemory().allocate(sizeof(CAmbisonicsDecodeEffect), Memory::kDefaultAlignment));
        new (_effect) CAmbisonicsDecodeEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
