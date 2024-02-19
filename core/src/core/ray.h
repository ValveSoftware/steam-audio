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

#include "box.h"
#include "hit.h"
#include "mesh.h"
#include "sphere.h"
#include "triangle.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Ray
// --------------------------------------------------------------------------------------------------------------------

// A single ray.
class Ray
{
public:
    Vector3f origin;
    Vector3f direction;

    Vector3f pointAtDistance(float distance) const
    {
        return origin + (distance * direction);
    }

    // Calculates the intersection of a ray with a triangle.
    float intersect(const Mesh& mesh,
                    int triangleIndex) const;

    // Checks whether a ray passes through a box, within a t interval
    // specified by minDistance and maxDistance. When the function returns,
    // minDistance and maxDistance will be set to the actual t interval in
    // which the ray passes through the box.
    bool intersect(const Box& box,
                   const Vector3f& reciprocalDirection,
                   const int* directionSigns,
                   float& minDistance,
                   float& maxDistance) const;

    // Calculates the intersection of a ray with a sphere.
    float intersect(const Sphere& sphere) const;
};

}
