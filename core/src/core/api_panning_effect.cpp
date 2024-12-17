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

#include "panning_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_panning_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CPanningEffect
// --------------------------------------------------------------------------------------------------------------------

CPanningEffect::CPanningEffect(CContext* context,
                               IPLAudioSettings* audioSettings,
                               IPLPanningEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _speakerLayout = SpeakerLayout(static_cast<SpeakerLayoutType>(effectSettings->speakerLayout.type),
                                        effectSettings->speakerLayout.numSpeakers,
                                        reinterpret_cast<Vector3f*>(effectSettings->speakerLayout.speakers));

    PanningEffectSettings _effectSettings{};
    _effectSettings.speakerLayout = &_speakerLayout;

    new (&mHandle) Handle<PanningEffect>(ipl::make_shared<PanningEffect>(_effectSettings), _context);
}

IPanningEffect* CPanningEffect::retain()
{
    mHandle.retain();
    return this;
}

void CPanningEffect::release()
{
    if (mHandle.release())
    {
        this->~CPanningEffect();
        gMemory().free(this);
    }
}

void CPanningEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CPanningEffect::apply(IPLPanningEffectParams* params,
                                          IPLAudioBuffer* in,
                                          IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    PanningEffectParams _params{};
    _params.direction = reinterpret_cast<const Vector3f*>(&params->direction);

    return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}

IPLint32 CPanningEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CPanningEffect::getTail(IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createPanningEffect(IPLAudioSettings* audioSettings,
                                       IPLPanningEffectSettings* effectSettings,
                                       IPanningEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CPanningEffect*>(gMemory().allocate(sizeof(CPanningEffect), Memory::kDefaultAlignment));
        new (_effect) CPanningEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
