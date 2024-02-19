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

#include "panning_effect.h"
#include "polar_vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// PanningEffect
// --------------------------------------------------------------------------------------------------------------------

// Intermediate data for 2D pairwise constant-power panning.
struct PanningData
{
    int     speakerIndices[2];      // The two speaker indices we want to pan between.
    float   angleBetweenSpeakers;   // The angle between the speakers.
    float   dPhi;                   // The angle between the first speaker and the source.
};

PanningEffect::PanningEffect(const PanningEffectSettings& settings)
    : mSpeakerLayout(*settings.speakerLayout)
{
    reset();
}

void PanningEffect::reset()
{
    mPrevDirection = Vector3f::kZero;
}

AudioEffectState PanningEffect::apply(const PanningEffectParams& params,
                                      const AudioBuffer& in,
                                      AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);
    assert(out.numChannels() == mSpeakerLayout.numSpeakers);

    PanningData panningData{};
    PanningData prevPanningData{};
    if (mSpeakerLayout.type == SpeakerLayoutType::Quadraphonic ||
        mSpeakerLayout.type == SpeakerLayoutType::FivePointOne ||
        mSpeakerLayout.type == SpeakerLayoutType::SevenPointOne)
    {
        // We will be using pairwise constant-power panning for this speaker layout,
        // so precalculate some intermediate data instead of recalculating this
        // information once for each output channel.
        calcPairwisePanningData(*params.direction, mSpeakerLayout, panningData);
        calcPairwisePanningData(mPrevDirection, mSpeakerLayout, prevPanningData);
    }

    for (auto i = 0; i < out.numChannels(); ++i)
    {
        auto weight = panningWeight(*params.direction, mSpeakerLayout, i, &panningData);
        auto weightPrev = panningWeight(mPrevDirection, mSpeakerLayout, i, &prevPanningData);

        for (auto j = 0; j < in.numSamples(); ++j)
        {
            // Crossfade between the panning coefficients for the previous frame and the
            // current frame.
            auto alpha = static_cast<float>(i) / static_cast<float>(in.numSamples());
            auto blendedWeight = alpha * weight + (1.0f - alpha) * weightPrev;

            out[i][j] = blendedWeight * in[0][j];
        }
    }

    mPrevDirection = *params.direction;

    return AudioEffectState::TailComplete;
}

AudioEffectState PanningEffect::tail(AudioBuffer& out)
{
    out.makeSilent();
    return AudioEffectState::TailComplete;
}

float PanningEffect::panningWeight(const Vector3f& direction,
                                   const SpeakerLayout& speakerLayout,
                                   int index,
                                   const PanningData* panningData /* = nullptr */)
{
    switch (speakerLayout.type)
    {
    case SpeakerLayoutType::Mono:
        return 1.0f;

    case SpeakerLayoutType::Stereo:
        return stereoPanningWeight(direction, index);

    case SpeakerLayoutType::Quadraphonic:
    case SpeakerLayoutType::FivePointOne:
    case SpeakerLayoutType::SevenPointOne:
        return pairwisePanningWeight(direction, speakerLayout, index, panningData);

    default:
        return firstOrderPanningWeight(direction, speakerLayout, index);
    }
}

float PanningEffect::stereoPanningWeight(const Vector3f& direction,
                                         int index)
{
    auto horizontal = Vector3f::unitVector(Vector3f(direction.x(), 0.0f, direction.z()));
    auto p = horizontal.x();
    auto q = (p + 1) * (Math::kPi / 4.0f);
    auto weight = (index == 0) ? cosf(q) : sinf(q);
    return weight;
}

float PanningEffect::firstOrderPanningWeight(const Vector3f& direction,
                                             const SpeakerLayout& speakerLayout,
                                             int index)
{
    auto sourceDirection = Vector3f::unitVector(direction);
    auto speakerDirection = Vector3f::unitVector(speakerLayout.speakers[index]);
    auto numSpeakers = speakerLayout.numSpeakers;
    auto cosTheta = Vector3f::dot(sourceDirection, speakerDirection);

    return (1.0f + cosTheta) / numSpeakers;
}

float PanningEffect::secondOrderPanningWeight(const Vector3f& direction,
                                              const SpeakerLayout& speakerLayout,
                                              int index)
{
    auto sourceDirection = Vector3f::unitVector(direction);
    auto speakerDirection = Vector3f::unitVector(speakerLayout.speakers[index]);
    auto numSpeakers = speakerLayout.numSpeakers;
    auto cosTheta = Vector3f::dot(sourceDirection, speakerDirection);

    return (4.0f * cosTheta * cosTheta + 2.0f * cosTheta - 1.0f) / numSpeakers;
}

float PanningEffect::pairwisePanningWeight(const Vector3f& direction,
                                           const SpeakerLayout& speakerLayout,
                                           int index,
                                           const PanningData* panningData /* = nullptr */)
{
    PanningData panningDataForDirection{};
    if (!panningData)
    {
        // No intermediate data was provided by the caller, so calculate it now.
        calcPairwisePanningData(direction, speakerLayout, panningDataForDirection);
        panningData = &panningDataForDirection;
    }

    if (index == panningData->speakerIndices[0])
        return cosf((panningData->dPhi / panningData->angleBetweenSpeakers) * (Math::kPi / 2));
    else if (index == panningData->speakerIndices[1])
        return sinf((panningData->dPhi / panningData->angleBetweenSpeakers) * (Math::kPi / 2));
    else
        return 0.0f;
}

void PanningEffect::calcPairwisePanningData(const Vector3f& direction,
                                            const SpeakerLayout& speakerLayout,
                                            PanningData& panningData)
{
    // Find the azimuth of the source direction, relative to forward.
    auto polarDirection = SphericalVector3f(direction);
    auto phi = std::max(0.0f, polarDirection.azimuth);

    // Figure out which speaker pair we want to pan between, along with angle
    // between speakers.
    switch (speakerLayout.type)
    {
    case SpeakerLayoutType::Quadraphonic:
        if (phi <= (Math::kPi / 4) || 7 * (Math::kPi / 4) < phi)
        {
            panningData.speakerIndices[0] = 0; // fl
            panningData.speakerIndices[1] = 1; // fr
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        else if ((Math::kPi / 4) < phi && phi <= 3 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 2; // rl
            panningData.speakerIndices[1] = 0; // fl
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        else if (3 * (Math::kPi / 4) < phi && phi <= 5 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 3; // rr
            panningData.speakerIndices[1] = 2; // rl
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        else if (5 * (Math::kPi / 4) < phi && phi <= 7 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 1; // fr
            panningData.speakerIndices[1] = 3; // rr
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        break;
    case SpeakerLayoutType::FivePointOne:
        if (0.0f <= phi && phi < (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 0; // fl
            panningData.speakerIndices[1] = 2; // c
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        else if ((Math::kPi / 4) <= phi && phi < 3 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 4; // rl
            panningData.speakerIndices[1] = 0; // fl
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        else if (3 * (Math::kPi / 4) <= phi && phi < 5 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 5; // rr
            panningData.speakerIndices[1] = 4; // rl
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        else if (5 * (Math::kPi / 4) <= phi && phi < 7 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 1; // fr
            panningData.speakerIndices[1] = 5; // rr
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        else if (7 * (Math::kPi / 4) <= phi)
        {
            panningData.speakerIndices[0] = 2; // c
            panningData.speakerIndices[1] = 1; // fr
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        break;
    case SpeakerLayoutType::SevenPointOne:
        if (0.0f <= phi && phi < (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 0; // fl
            panningData.speakerIndices[1] = 2; // c
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        else if ((Math::kPi / 4) <= phi && phi < 2 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 6; // sl
            panningData.speakerIndices[1] = 0; // fl
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        else if (2 * (Math::kPi / 4) <= phi && phi < 3 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 4; // rl
            panningData.speakerIndices[1] = 6; // sl
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        else if (3 * (Math::kPi / 4) <= phi && phi < 5 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 5; // rr
            panningData.speakerIndices[1] = 4; // rl
            panningData.angleBetweenSpeakers = Math::kPi / 2;
        }
        else if (5 * (Math::kPi / 4) <= phi && phi < 6 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 7; // sr
            panningData.speakerIndices[1] = 5; // rr
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        else if (6 * (Math::kPi / 4) <= phi && phi < 7 * (Math::kPi / 4))
        {
            panningData.speakerIndices[0] = 1; // fr
            panningData.speakerIndices[1] = 7; // sr
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        else if (7 * (Math::kPi / 4) <= phi)
        {
            panningData.speakerIndices[0] = 2; // c
            panningData.speakerIndices[1] = 1; // fr
            panningData.angleBetweenSpeakers = Math::kPi / 4;
        }
        break;
    default:
        return;
    }

    // Calculate the angle between first speaker and source.
    const auto* speakers = SpeakerLayout::speakersForLayout(speakerLayout.type);
    const auto& firstSpeaker = speakers[panningData.speakerIndices[0]];
    panningData.dPhi = acosf(Vector3f::dot(Vector3f::unitVector(direction), Vector3f::unitVector(firstSpeaker)));
}

}
