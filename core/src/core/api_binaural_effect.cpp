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

#include "binaural_effect.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_hrtf.h"
#include "api_binaural_effect.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CBinauralEffect
// --------------------------------------------------------------------------------------------------------------------

IPLint32 CBinauralEffect::getTailSize()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return 0;

    return _effect->numTailSamplesRemaining();
}

IPLAudioEffectState CBinauralEffect::getTail(IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    return static_cast<IPLAudioEffectState>(_effect->tail(_out));
}

CBinauralEffect::CBinauralEffect(CContext* context,
                                 IPLAudioSettings* audioSettings,
                                 IPLBinauralEffectSettings* effectSettings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _hrtf = reinterpret_cast<CHRTF*>(effectSettings->hrtf)->mHandle.get();
    if (!_hrtf)
        throw Exception(Status::Failure);

    AudioSettings _audioSettings{};
    _audioSettings.samplingRate = audioSettings->samplingRate;
    _audioSettings.frameSize = audioSettings->frameSize;

    BinauralEffectSettings _effectSettings{};
    _effectSettings.hrtf = _hrtf.get();

    new (&mHandle) Handle<BinauralEffect>(ipl::make_shared<BinauralEffect>(_audioSettings, _effectSettings), _context);
}

IBinauralEffect* CBinauralEffect::retain()
{
    mHandle.retain();
    return this;
}

void CBinauralEffect::release()
{
    if (mHandle.release())
    {
        this->~CBinauralEffect();
        gMemory().free(this);
    }
}

void CBinauralEffect::reset()
{
    auto _effect = mHandle.get();
    if (!_effect)
        return;

    _effect->reset();
}

IPLAudioEffectState CBinauralEffect::apply(IPLBinauralEffectParams* params,
                                           IPLAudioBuffer* in,
                                           IPLAudioBuffer* out)
{
    auto _effect = mHandle.get();
    if (!_effect)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    auto _hrtf = reinterpret_cast<CHRTF*>(params->hrtf)->mHandle.get();
    if (!_hrtf)
        return IPL_AUDIOEFFECTSTATE_TAILCOMPLETE;

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    BinauralEffectParams _params{};
    _params.direction = reinterpret_cast<const Vector3f*>(&params->direction);
    _params.interpolation = static_cast<HRTFInterpolation>(params->interpolation);
    _params.spatialBlend = params->spatialBlend;
    _params.hrtf = _hrtf.get();

    if (Context::isCallerAPIVersionAtLeast(4, 1))
    {
        _params.peakDelays = params->peakDelays;
    }

    return static_cast<IPLAudioEffectState>(_effect->apply(_params, _in, _out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createBinauralEffect(IPLAudioSettings* audioSettings,
                                        IPLBinauralEffectSettings* effectSettings,
                                        IBinauralEffect** effect)
{
    if (!audioSettings || !effectSettings || !effect)
        return IPL_STATUS_FAILURE;

    if (audioSettings->samplingRate <= 0 || audioSettings->frameSize <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _effect = reinterpret_cast<CBinauralEffect*>(gMemory().allocate(sizeof(CBinauralEffect), Memory::kDefaultAlignment));
        new (_effect) CBinauralEffect(this, audioSettings, effectSettings);
        *effect = _effect;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
