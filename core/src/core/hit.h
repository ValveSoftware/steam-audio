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

#include "material.h"
#include "vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Hit
// --------------------------------------------------------------------------------------------------------------------

class Hit
{
public:
    float distance; // The distance along the ray at which the intersection occurs.
    int triangleIndex; // The index of the triangle that was hit.
    int objectIndex; // The index of the scene object that was hit.
    int materialIndex; // The material index at the hit point.
    Vector3f normal; // The surface normal at the hit point.
    const Material* material; // The material at the hit point.

    Hit()
        : distance(std::numeric_limits<float>::infinity())
        , triangleIndex(-1)
        , objectIndex(-1)
        , materialIndex(-1)
        , normal(Vector3f::kZero)
        , material(nullptr)
    {}

    bool isValid() const
    {
        return (distance < std::numeric_limits<float>::infinity());
    }
};

}
