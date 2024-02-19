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
// Sphere
// --------------------------------------------------------------------------------------------------------------------

// Represents a sphere in three dimensions.
class Sphere
{
public:
    Vector3f center; // Center of the sphere.
    float radius; // Radius of the sphere.

    // Constructs a sphere whose center is at the origin, and whose radius is zero.
    Sphere()
        : center(Vector3f::kZero)
        , radius(0.0f)
    {}

    // Constructs a sphere given a center and radius.
    Sphere(const Vector3f& center,
           float radius)
        : center(center)
        , radius(radius)
    {}

    // Checks whether the sphere contains a point.
    bool contains(const Vector3f& point) const
    {
        return (point - center).lengthSquared() <= radius * radius;
    }
};

// Computes the minimal bounding sphere that fully contains sphereA and sphereB.
inline Sphere computeBoundingSphere(const Sphere& sphereA,
                                    const Sphere& sphereB)
{
    Sphere boundingSphere;

    auto vectorBetweenCenters = sphereA.center - sphereB.center;
    auto sqDistBetweenCenters = vectorBetweenCenters.lengthSquared();
    auto radiiDifference = sphereA.radius - sphereB.radius;
    auto sqRadiiDifference = radiiDifference * radiiDifference;
    auto biggerSphere = sphereA.radius > sphereB.radius ? sphereA : sphereB;

    // Is one sphere contained within the other?
    if (sqDistBetweenCenters < sqRadiiDifference)
        return biggerSphere;

    auto boundingSphereRadius = (sphereA.radius + sphereB.radius + sqrtf(sqDistBetweenCenters)) * 0.5f;
    auto centerAxis = Vector3f::unitVector(vectorBetweenCenters);
    auto boundingSphereCenter = sphereB.center + centerAxis * (boundingSphereRadius - sphereB.radius);

    boundingSphere.radius = boundingSphereRadius;
    boundingSphere.center = boundingSphereCenter;

    return boundingSphere;
}

}
