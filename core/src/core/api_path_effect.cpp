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

#include "path_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_hrtf.h"
#include "api_path_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CPathEffect
// --------------------------------------------------------------------------------------------------------------------

IPLint32 CPathEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CPathEffect::getTail(IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}

CPathEffect::CPathEffect(CContext* context,
                         IPLAudioSettings* audioSettings,
                         IPLPathEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    AudioSettings _audioSettings{};
    _audioSettings.samplingRate = audioSettings->samplingRate;
    _audioSettings.frameSize = audioSettings->frameSize;

    SpeakerLayout _speakerLayout{};

    PathEffectSettings _effectSettings{};
    _effectSettings.maxOrder = effectSettings->maxOrder;

    if (Context::isCallerAPIVersionAtLeast(4, 4))
    {
        _effectSettings.spatialize = (effectSettings->spatialize == IPL_TRUE);

        _speakerLayout = SpeakerLayout(static_cast<SpeakerLayoutType>(effectSettings->speakerLayout.type),
                                       effectSettings->speakerLayout.numSpeakers,
                                       reinterpret_cast<Vector3f*>(effectSettings->speakerLayout.speakers));

        _effectSettings.speakerLayout = &_speakerLayout;

        _effectSettings.hrtf = effectSettings->hrtf ? reinterpret_cast<CHRTF*>(effectSettings->hrtf)->mHandle.get().get() : nullptr;
    }

    new (&mHandle) Handle<PathEffect>(ipl::make_shared<PathEffect>(_audioSettings, _effectSettings), _context);
}

IPathEffect* CPathEffect::retain()
{
    mHandle.retain();
    return this;
}

void CPathEffect::release()
{
    if (mHandle.release())
    {
        this->~CPathEffect();
        gMemory().free(this);
    }
}

void CPathEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CPathEffect::apply(IPLPathEffectParams* params,
                                       IPLAudioBuffer* in,
                                       IPLAudioBuffer* out)
{
    if (!params)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    PathEffectParams _params{};
    _params.eqCoeffs = params->eqCoeffs;
    _params.shCoeffs = params->shCoeffs;
    _params.order = params->order;

    if (Context::isCallerAPIVersionAtLeast(4, 4))
    {
        _params.binaural = (params->binaural == IPL_TRUE);
        _params.hrtf = params->hrtf ? reinterpret_cast<CHRTF*>(params->hrtf)->mHandle.get().get() : nullptr;
        _params.listener = reinterpret_cast<const CoordinateSpace3f*>(&params->listener);
    }

    return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createPathEffect(IPLAudioSettings* audioSettings,
                                    IPLPathEffectSettings* effectSettings,
                                    IPathEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    if (effectSettings->maxOrder < 0 || 3 < effectSettings->maxOrder)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CPathEffect*>(gMemory().allocate(sizeof(CPathEffect), Memory::kDefaultAlignment));
        new (_effect) CPathEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
