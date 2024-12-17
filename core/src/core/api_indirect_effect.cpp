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

#include "indirect_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_tan_device.h"
#include "api_indirect_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CReflectionEffect
// --------------------------------------------------------------------------------------------------------------------

CReflectionEffect::CReflectionEffect(CContext* context,
                                     IPLAudioSettings* audioSettings,
                                     IPLReflectionEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    AudioSettings _audioSettings{};
    _audioSettings.samplingRate = audioSettings->samplingRate;
    _audioSettings.frameSize = audioSettings->frameSize;

    IndirectEffectSettings _effectSettings{};
    _effectSettings.type = static_cast<IndirectEffectType>(effectSettings->type);
    _effectSettings.numChannels = effectSettings->numChannels;
    _effectSettings.irSize = effectSettings->irSize;

    new (&mHandle) Handle<IndirectEffect>(ipl::make_shared<IndirectEffect>(_audioSettings, _effectSettings), _context);
}

IReflectionEffect* CReflectionEffect::retain()
{
    mHandle.retain();
    return this;
}

void CReflectionEffect::release()
{
    if (mHandle.release())
    {
        this->~CReflectionEffect();
        gMemory().free(this);
    }
}

void CReflectionEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CReflectionEffect::apply(IPLReflectionEffectParams* params,
                                             IPLAudioBuffer* in,
                                             IPLAudioBuffer* out,
                                             IReflectionMixer* mixer)
{
    if (!params)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    auto _tan = (params->tanDevice) ? reinterpret_cast<CTrueAudioNextDevice*>(params->tanDevice)->mHandle.get() : nullptr;

    IndirectEffectParams _params{};
    _params.fftIR = reinterpret_cast<TripleBuffer<OverlapSaveFIR>*>(params->ir);
    _params.reverb = reinterpret_cast<const Reverb*>(params->reverbTimes);
    _params.eqCoeffs = params->eq;
    _params.delay = params->delay;
    _params.numChannels = params->numChannels;
    _params.numSamples = params->irSize;
    _params.tan = _tan.get();
    _params.slot = params->tanSlot;

    auto _mixer = (mixer) ? reinterpret_cast<CReflectionMixer*>(mixer)->mHandle.get() : nullptr;

    if (_mixer)
        return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, *_mixer));
    else
        return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}

IPLint32 CReflectionEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CReflectionEffect::getTail(IPLAudioBuffer* out, IReflectionMixer* mixer)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    auto _mixer = (mixer) ? reinterpret_cast<CReflectionMixer*>(mixer)->mHandle.get() : nullptr;

    if (mixer)
        return static_cast<IPLAudioEffectState>(_effect->tail(*_mixer));
    else
        return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}


// --------------------------------------------------------------------------------------------------------------------
// CReflectionMixer
// --------------------------------------------------------------------------------------------------------------------

CReflectionMixer::CReflectionMixer(CContext* context,
                                   IPLAudioSettings* audioSettings,
                                   IPLReflectionEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    AudioSettings _audioSettings{};
    _audioSettings.samplingRate = audioSettings->samplingRate;
    _audioSettings.frameSize = audioSettings->frameSize;

    IndirectEffectSettings _effectSettings{};
    _effectSettings.type = static_cast<IndirectEffectType>(effectSettings->type);
    _effectSettings.numChannels = effectSettings->numChannels;

    new (&mHandle) Handle<IndirectMixer>(ipl::make_shared<IndirectMixer>(_audioSettings, _effectSettings), _context);
}

IReflectionMixer* CReflectionMixer::retain()
{
    mHandle.retain();
    return this;
}

void CReflectionMixer::release()
{
    if (mHandle.release())
    {
        this->~CReflectionMixer();
        gMemory().free(this);
    }
}

void CReflectionMixer::reset()
{
    auto _mixer = mHandle.get();
    if (!_mixer)
        return;

    _mixer->reset();
}

IPLAudioEffectState CReflectionMixer::apply(IPLReflectionEffectParams* params,
                                            IPLAudioBuffer* out)
{
    if (!params)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;;

    auto _mixer = mHandle.get();
    if (!_mixer)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    auto _tan = (params->tanDevice) ? reinterpret_cast<CTrueAudioNextDevice*>(params->tanDevice)->mHandle.get() : nullptr;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    IndirectMixerParams _params{};
    _params.numChannels = params->numChannels;
    _params.tan = _tan.get();

    _mixer->apply(_params, _out);

    return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createReflectionEffect(IPLAudioSettings* audioSettings,
                                          IPLReflectionEffectSettings* effectSettings,
                                          IReflectionEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CReflectionEffect*>(gMemory().allocate(sizeof(CReflectionEffect), Memory::kDefaultAlignment));
        new (_effect) CReflectionEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLerror CContext::createReflectionMixer(IPLAudioSettings* audioSettings,
                                         IPLReflectionEffectSettings* effectSettings,
                                         IReflectionMixer** mixer)
{
    if (!audioSettings || !effectSettings || !mixer)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _mixer = reinterpret_cast<CReflectionMixer*>(gMemory().allocate(sizeof(CReflectionMixer), Memory::kDefaultAlignment));
        new (_mixer) CReflectionMixer(this, audioSettings, effectSettings);
        *mixer = _mixer;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
