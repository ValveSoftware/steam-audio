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

#include "distance_attenuation.h"
#include "propagation_medium.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DistanceAttenuationModel
// --------------------------------------------------------------------------------------------------------------------

const float DistanceAttenuationModel::kDefaultMinDistance = 1.0f;

DistanceAttenuationModel::DistanceAttenuationModel()
    : DistanceAttenuationModel(kDefaultMinDistance, nullptr, nullptr)
{}

DistanceAttenuationModel::DistanceAttenuationModel(float minDistance,
                                                   DistanceAttenuationCallback callback,
                                                   void* userData)
    : minDistance(minDistance)
    , callback(callback)
    , userData(userData)
    , dirty(false)
{}

bool DistanceAttenuationModel::isDefault() const
{
    return (minDistance == kDefaultMinDistance && callback == nullptr);
}

float DistanceAttenuationModel::evaluate(float distance) const
{
    if (callback)
    {
        return callback(distance, userData);
    }
    else
    {
        return 1.0f / std::max(distance, minDistance);
    }
}

void DistanceAttenuationModel::generateCorrectionCurve(const DistanceAttenuationModel& from,
                                                       const DistanceAttenuationModel& to,
                                                       int samplingRate,
                                                       int numSamples,
                                                       float* curve)
{
    for (auto i = 0; i < numSamples; ++i)
    {
        auto t = static_cast<float>(i) / static_cast<float>(samplingRate);
        auto r = t * PropagationMedium::kSpeedOfSound;
        curve[i] = to.evaluate(r) / from.evaluate(r);
    }
}

bool operator==(const DistanceAttenuationModel& lhs,
                const DistanceAttenuationModel& rhs)
{
    return (lhs.minDistance == rhs.minDistance &&
            lhs.callback == rhs.callback &&
            lhs.userData == rhs.userData);
}

bool operator!=(const DistanceAttenuationModel& lhs,
                const DistanceAttenuationModel& rhs)
{
    return !(lhs == rhs);
}

}
