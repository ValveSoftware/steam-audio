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

#include "memory_allocator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DistanceAttenuationModel
// --------------------------------------------------------------------------------------------------------------------

typedef float (IPL_CALLBACK *DistanceAttenuationCallback)(float distance,
                                                          void* userData);

class DistanceAttenuationModel
{
public:
    float minDistance;
    DistanceAttenuationCallback callback;
    void* userData;
    mutable bool dirty;

    DistanceAttenuationModel();

    DistanceAttenuationModel(float minDistance,
                             DistanceAttenuationCallback callback,
                             void* userData);

    bool isDefault() const;

    float evaluate(float distance) const;

    static void generateCorrectionCurve(const DistanceAttenuationModel& from,
                                        const DistanceAttenuationModel& to,
                                        int samplingRate,
                                        int numSamples,
                                        float* curve);

private:
    static const float kDefaultMinDistance;
};

bool operator==(const DistanceAttenuationModel& lhs,
                const DistanceAttenuationModel& rhs);

bool operator!=(const DistanceAttenuationModel& lhs,
                const DistanceAttenuationModel& rhs);
}
