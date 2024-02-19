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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsRotateEffect
// --------------------------------------------------------------------------------------------------------------------

AmbisonicsRotateEffect::AmbisonicsRotateEffect(const AudioSettings& audioSettings,
                                               const AmbisonicsRotateEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mMaxOrder(effectSettings.maxOrder)
    , mNumAmbisonicsChannels(SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder))
    , mRotations{SHRotation(effectSettings.maxOrder), SHRotation(effectSettings.maxOrder)}
    , mCoeffs(mNumAmbisonicsChannels)
    , mRotatedCoeffs(mNumAmbisonicsChannels)
    , mRotatedCoeffsPrev(mNumAmbisonicsChannels)
{
    reset();
}

void AmbisonicsRotateEffect::reset()
{
    mCurrent = 0;

    mRotations[0].setRotation(CoordinateSpace3f{});
    mRotations[1].setRotation(CoordinateSpace3f{});
}

AudioEffectState AmbisonicsRotateEffect::apply(const AmbisonicsRotateEffectParams& params,
                                               const AudioBuffer& in,
                                               AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == SphericalHarmonics::numCoeffsForOrder(params.order));
    assert(out.numChannels() == SphericalHarmonics::numCoeffsForOrder(params.order));

    auto order = std::min(params.order, mMaxOrder);

    auto previous = 1 - mCurrent;

    mRotations[mCurrent].setRotation(*params.orientation);

    for (auto i = 0; i < mFrameSize; ++i)
    {
        for (auto l = 0, j = 0; l <= order; ++l)
        {
            for (auto m = -l; m <= l; ++m, ++j)
            {
                mCoeffs[j] = in[j][i];
            }
        }

        mRotations[mCurrent].apply(order, mCoeffs.data(), mRotatedCoeffs.data());
        mRotations[previous].apply(order, mCoeffs.data(), mRotatedCoeffsPrev.data());

        auto weight = static_cast<float>(i) / static_cast<float>(mFrameSize);

        for (auto l = 0, j = 0; l <= order; ++l)
        {
            for (auto m = -l; m <= l; ++m, ++j)
            {
                out[j][i] = (1.0f - weight) * mRotatedCoeffsPrev[j] + weight * mRotatedCoeffs[j];
            }
        }
    }

    mCurrent = 1 - mCurrent;

    return AudioEffectState::TailComplete;
}

AudioEffectState AmbisonicsRotateEffect::tail(AudioBuffer& out)
{
    out.makeSilent();
    return AudioEffectState::TailComplete;
}

}
