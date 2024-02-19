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

#include "speaker_layout.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SpeakerLayout
// --------------------------------------------------------------------------------------------------------------------

const Vector3f SpeakerLayout::sMonoSpeakers[1] = {
    Vector3f(0.0f, 0.0f, 0.0f)
};

const Vector3f SpeakerLayout::sStereoSpeakers[2] = {
    Vector3f(-1.0f, 0.0f, 0.0f),
    Vector3f(1.0f, 0.0f, 0.0f)
};

const Vector3f SpeakerLayout::sQuadSpeakers[4] = {
    Vector3f(-1.0f, 0.0f, -1.0f),
    Vector3f(1.0f, 0.0f, -1.0f),
    Vector3f(-1.0f, 0.0f, 1.0f),
    Vector3f(1.0f, 0.0f, 1.0f)
};

const Vector3f SpeakerLayout::s51Speakers[6] = {
    Vector3f(-1.0f, 0.0f, -1.0f),
    Vector3f(1.0f, 0.0f, -1.0f),
    Vector3f(0.0f, 0.0f, -1.0f),
    Vector3f(0.0f, 0.0f, -1.0f),
    Vector3f(-1.0f, 0.0f, 1.0f),
    Vector3f(1.0f, 0.0f, 1.0f)
};

const Vector3f SpeakerLayout::s71Speakers[8] = {
    Vector3f(-1.0f, 0.0f, -1.0f),
    Vector3f(1.0f, 0.0f, -1.0f),
    Vector3f(0.0f, 0.0f, -1.0f),
    Vector3f(0.0f, 0.0f, -1.0f),
    Vector3f(-1.0f, 0.0f, 1.0f),
    Vector3f(1.0f, 0.0f, 1.0f),
    Vector3f(-1.0f, 0.0f, 0.0f),
    Vector3f(1.0f, 0.0f, 0.0f)
};

SpeakerLayout::SpeakerLayout(SpeakerLayoutType type)
    : SpeakerLayout(type, 0, nullptr)
{}

SpeakerLayout::SpeakerLayout(int numSpeakers,
                             const Vector3f* speakers)
    : SpeakerLayout(SpeakerLayoutType::Custom, numSpeakers, speakers)
{}

SpeakerLayout::SpeakerLayout(SpeakerLayoutType type,
                             int numSpeakers,
                             const Vector3f* speakers)
    : type(type)
    , numSpeakers(numSpeakers)
    , speakers(speakers)
{
    if (type != SpeakerLayoutType::Custom)
    {
        this->numSpeakers = numSpeakersForLayout(type);
        this->speakers = speakersForLayout(type);
    }
}

int SpeakerLayout::numSpeakersForLayout(SpeakerLayoutType type)
{
    switch (type)
    {
    case SpeakerLayoutType::Mono:
        return 1;
    case SpeakerLayoutType::Stereo:
        return 2;
    case SpeakerLayoutType::Quadraphonic:
        return 4;
    case SpeakerLayoutType::FivePointOne:
        return 6;
    case SpeakerLayoutType::SevenPointOne:
        return 8;
    default:
        return 0;
    }
}

const Vector3f* SpeakerLayout::speakersForLayout(SpeakerLayoutType type)
{
    switch (type)
    {
    case SpeakerLayoutType::Mono:
        return sMonoSpeakers;
    case SpeakerLayoutType::Stereo:
        return sStereoSpeakers;
    case SpeakerLayoutType::Quadraphonic:
        return sQuadSpeakers;
    case SpeakerLayoutType::FivePointOne:
        return s51Speakers;
    case SpeakerLayoutType::SevenPointOne:
        return s71Speakers;
    default:
        return nullptr;
    }
}

}
