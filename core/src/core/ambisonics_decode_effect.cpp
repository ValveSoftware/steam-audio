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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsDecodeEffect
// --------------------------------------------------------------------------------------------------------------------

AmbisonicsDecodeEffect::AmbisonicsDecodeEffect(const AudioSettings& audioSettings,
                                               const AmbisonicsDecodeEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mSpeakerLayout(*effectSettings.speakerLayout)
    , mMaxOrder(effectSettings.maxOrder)
    , mRotated(SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder), audioSettings.frameSize)
{
    AmbisonicsPanningEffectSettings panningSettings{};
    panningSettings.speakerLayout = effectSettings.speakerLayout;
    panningSettings.maxOrder = effectSettings.maxOrder;

    mPanningEffect = make_unique<AmbisonicsPanningEffect>(audioSettings, panningSettings);

    if (effectSettings.hrtf)
    {
        AmbisonicsBinauralEffectSettings binauralSettings{};
        binauralSettings.maxOrder = effectSettings.maxOrder;
        binauralSettings.hrtf = effectSettings.hrtf;

        mBinauralEffect = make_unique<AmbisonicsBinauralEffect>(audioSettings, binauralSettings);
    }

    AmbisonicsRotateEffectSettings rotateSettings{};
    rotateSettings.maxOrder = effectSettings.maxOrder;

    mRotateEffect = make_unique<AmbisonicsRotateEffect>(audioSettings, rotateSettings);

    reset();
}

void AmbisonicsDecodeEffect::reset()
{
    mPanningEffect->reset();

    if (mBinauralEffect)
    {
        mBinauralEffect->reset();
    }

    mRotateEffect->reset();

    mPrevBinaural = false;
}

AudioEffectState AmbisonicsDecodeEffect::apply(const AmbisonicsDecodeEffectParams& params,
                                               const AudioBuffer& in,
                                               AudioBuffer& out)
{
    AmbisonicsRotateEffectParams rotateParams{};
    rotateParams.orientation = params.orientation;
    rotateParams.order = params.order;

    mRotateEffect->apply(rotateParams, in, mRotated);

    auto binaural = (params.binaural && mSpeakerLayout.type == SpeakerLayoutType::Stereo && params.hrtf && mBinauralEffect);

    if (binaural && !mPrevBinaural)
    {
        mPanningEffect->reset();
    }
    else if (!binaural && mPrevBinaural && mBinauralEffect)
    {
        mBinauralEffect->reset();
    }

    AudioEffectState effectState = AudioEffectState::TailComplete;

    if (binaural)
    {
        AmbisonicsBinauralEffectParams binauralParams{};
        binauralParams.hrtf = params.hrtf;
        binauralParams.order = params.order;

        effectState = mBinauralEffect->apply(binauralParams, mRotated, out);
    }
    else
    {
        AmbisonicsPanningEffectParams panningParams{};
        panningParams.order = params.order;

        effectState = mPanningEffect->apply(panningParams, mRotated, out);
    }

    mPrevBinaural = binaural;

    return effectState;
}

AudioEffectState AmbisonicsDecodeEffect::tail(AudioBuffer& out)
{
    if (mPrevBinaural)
        return mBinauralEffect->tail(out);
    else
        return mPanningEffect->tail(out);
}

int AmbisonicsDecodeEffect::numTailSamplesRemaining() const
{
    if (mPrevBinaural)
        return mBinauralEffect->numTailSamplesRemaining();
    else
        return mPanningEffect->numTailSamplesRemaining();
}

}
