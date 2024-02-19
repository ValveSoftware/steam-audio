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

#pragma once

#include "vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SpeakerLayout
// --------------------------------------------------------------------------------------------------------------------

// Supported speaker layouts.
enum class SpeakerLayoutType
{
    Mono,
    Stereo,
    Quadraphonic,
    FivePointOne,
    SevenPointOne,
    Custom
};

// Describes a speaker layout.
class SpeakerLayout
{
public:
    SpeakerLayoutType type;
    int numSpeakers;
    const Vector3f* speakers; // #speakers

    SpeakerLayout() = default;

    // Automatically initializes numSpeakers and speakers for the given speaker layout.
    SpeakerLayout(SpeakerLayoutType type);

    // For specifying custom speaker layouts.
    SpeakerLayout(int numSpeakers,
                  const Vector3f* speakers);

    SpeakerLayout(SpeakerLayoutType type,
                  int numSpeakers,
                  const Vector3f* speakers);

    static int numSpeakersForLayout(SpeakerLayoutType type);

    static const Vector3f* speakersForLayout(SpeakerLayoutType type);

private:
    static const Vector3f sMonoSpeakers[1];
    static const Vector3f sStereoSpeakers[2];
    static const Vector3f sQuadSpeakers[4];
    static const Vector3f s51Speakers[6];
    static const Vector3f s71Speakers[8];
};

}
