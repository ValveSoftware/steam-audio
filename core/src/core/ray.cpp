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

#include "ray.h"

#include "math_functions.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Ray
// --------------------------------------------------------------------------------------------------------------------

// This implementation of ray-triangle intersection is based on the
// Moller-Trumbore algorithm:
//
//  Fast, Minimum Storage Ray/Triangle Intersection
//  T. Moller, B. Trumbore
//  Journal of Graphics Tools, 1997
//  http://www.cs.virginia.edu/~gfx/courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
float Ray::intersect(const Mesh& mesh,
                     int triangleIndex) const
{
    // Get the three vertices of the triangle.
    const auto& v0 = mesh.triangleVertex(triangleIndex, 0);
    const auto& v1 = mesh.triangleVertex(triangleIndex, 1);
    const auto& v2 = mesh.triangleVertex(triangleIndex, 2);

    // Calculate the vectors for the two edges sharing v0.
    auto edge1 = v1 - v0;
    auto edge2 = v2 - v0;

    // Begin calculating the determinant.
    auto p = Vector3f::cross(direction, edge2);
    auto determinant = Vector3f::dot(edge1, p);

    // If the determinant is zero, the ray is parallel to the plane of
    // the triangle, and we report no intersection.
    if (determinant == 0.0f)
        return std::numeric_limits<float>::infinity();

    auto inverseDeterminant = 1.0f / determinant;

    // Calculate the vector from v0 to the ray's origin.
    auto t = origin - v0;

    // Calculate the first barycentric coordinate of the hit point, and
    // check whether it lies within a valid interval.
    auto u = Vector3f::dot(t, p) * inverseDeterminant;
    if (u < 0.0f || 1.0f < u)
        return std::numeric_limits<float>::infinity();

    // Calculate the second barycentric coordinate of the hit point, and
    // check whether it lies within a valid interval.
    auto q = Vector3f::cross(t, edge1);
    auto v = Vector3f::dot(direction, q) * inverseDeterminant;
    if (v < 0.0f || 1.0f - u < v)
        return std::numeric_limits<float>::infinity();

    // We've found a valid hit point. Calculate the distance from the
    // ray's origin to the hit point.
    return Vector3f::dot(edge2, q) * inverseDeterminant;
}

// This implementation of ray-box intersection is based on the branchless slab
// test algorithm:
//
//  An Efficient and Robust Ray-Box Intersection Algorithm
//  A. Williams, S. Barrus, R. K. Morley, P. Shirley
//  Journal of Graphics Tools, 2005.
bool Ray::intersect(const Box& box,
                    const Vector3f& reciprocalDirection,
                    const int* directionSigns,
                    float& minDistance,
                    float& maxDistance) const
{
    // Calculate the t interval in which the ray passes through the x-slab
    // of the box.
    auto minimum = (box.coordinates(directionSigns[0] ^ 1).x() - origin.x()) * reciprocalDirection.x();
    auto maximum = (box.coordinates(directionSigns[0]).x() - origin.x()) * reciprocalDirection.x();
    minDistance = std::max(minDistance, minimum);
    maxDistance = std::min(maxDistance, maximum);

    // Calculate the t interval in which the ray passes through the y-slab
    // of the box.
    minimum = (box.coordinates(directionSigns[1] ^ 1).y() - origin.y()) * reciprocalDirection.y();
    maximum = (box.coordinates(directionSigns[1]).y() - origin.y()) * reciprocalDirection.y();
    minDistance = std::max(minDistance, minimum);
    maxDistance = std::min(maxDistance, maximum);

    // Calculate the t interval in which the ray passes through the z-slab
    // of the box.
    minimum = (box.coordinates(directionSigns[2] ^ 1).z() - origin.z()) * reciprocalDirection.z();
    maximum = (box.coordinates(directionSigns[2]).z() - origin.z()) * reciprocalDirection.z();
    minDistance = std::max(minDistance, minimum);
    maxDistance = std::min(maxDistance, maximum);

    // If the intersection of all three intervals is non-empty, the ray
    // passes through the box.
    return (minDistance <= maxDistance);
}

float Ray::intersect(const Sphere& sphere) const
{
    auto v = origin - sphere.center;
    auto r = sphere.radius;

    auto B = 2.0f * Vector3f::dot(v, direction);
    auto C = v.lengthSquared() - (r * r);
    auto D = (B * B) - (4.0f * C);

    if (D < 0.0f)
        return std::numeric_limits<float>::infinity();

    auto t = -0.5f * (B + sqrtf(D));
    return t;
}

}
