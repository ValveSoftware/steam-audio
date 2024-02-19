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

#include "ambisonics_encode_effect.h"

#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsEncodeEffect
// --------------------------------------------------------------------------------------------------------------------

AmbisonicsEncodeEffect::AmbisonicsEncodeEffect(const AmbisonicsEncodeEffectSettings& effectSettings)
    : mMaxOrder(effectSettings.maxOrder)
{
    reset();
}

void AmbisonicsEncodeEffect::reset()
{
    mPrevDirection = Vector3f::kZero;
}

AudioEffectState AmbisonicsEncodeEffect::apply(const AmbisonicsEncodeEffectParams& params,
                                               const AudioBuffer& in,
                                               AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);
    assert(out.numChannels() == SphericalHarmonics::numCoeffsForOrder(params.order));
    assert(params.direction);

    auto order = std::min(params.order, mMaxOrder);

    // Normalize the direction vector. If we've been passed a (nearly) zero vector,
    // then use the zero vector instead, it will automatically be projected to 0th order
    // Ambisonics by the SH evaluation code.
    auto directionLength = params.direction->length();
    auto normalizedDirection = (directionLength < Vector3f::kNearlyZero) ? Vector3f::kZero : (*params.direction / directionLength);

    for (auto l = 0, i = 0; l <= order; ++l)
    {
        for (auto m = -l; m <= l; ++m, ++i)
        {
            auto weight = SphericalHarmonics::evaluate(l, m, normalizedDirection);
            auto weightPrev = SphericalHarmonics::evaluate(l, m, mPrevDirection);

            for (auto j = 0; j < in.numSamples(); ++j)
            {
                // Crossfade between the coefficients for the previous frame and the current frame.
                auto alpha = static_cast<float>(i) / static_cast<float>(in.numSamples());
                auto blendedWeight = alpha * weight + (1.0f - alpha) * weightPrev;

                out[i][j] = blendedWeight * in[0][j];
            }
        }
    }

    mPrevDirection = normalizedDirection;

    return AudioEffectState::TailComplete;
}

AudioEffectState AmbisonicsEncodeEffect::tail(AudioBuffer& out)
{
    out.makeSilent();
    return AudioEffectState::TailComplete;
}

}
