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

#include "directivity.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Directivity
// --------------------------------------------------------------------------------------------------------------------

Directivity::Directivity()
    : Directivity(.0f, .0f, nullptr, nullptr)
{}

Directivity::Directivity(float dipoleWeight,
                         float dipolePower,
                         DirectivityCallback callback,
                         void* userData)
    : dipoleWeight(dipoleWeight)
    , dipolePower(dipolePower)
    , callback(callback)
    , userData(userData)
{}

float Directivity::evaluate(const Vector3f& direction) const
{
    if (callback)
    {
        return callback(direction, userData);
    }
    else
    {
        auto cosine = -direction.z();
        return powf(fabsf((1.0f - dipoleWeight) + dipoleWeight * cosine), dipolePower);
    }
}

float Directivity::evaluateAt(const Vector3f& point,
                              const CoordinateSpace3f& coordinates) const
{
    if (dipoleWeight == 0.0f && !callback)
        return 1.0f;

    auto worldSpaceDirection = Vector3f::unitVector(point - coordinates.origin);
    auto localSpaceDirection = coordinates.transformDirectionFromWorldToLocal(worldSpaceDirection);
    return evaluate(localSpaceDirection);
}

}