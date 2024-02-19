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

#include "float4.h"
#include "vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AABB<T>
// --------------------------------------------------------------------------------------------------------------------

// An axis-aligned box.
class Box
{
public:
    alignas(float4_t) Vector3f minCoordinates; // The minimum coordinates of any point in the box.
    alignas(float4_t) Vector3f maxCoordinates; // The maximum coordinates of any point in the box.

    // The default constructor creates a box with minimum coordinates at +infinity, and maximum coordinates at
    // -infinity. This is a box that contains no points.
    Box()
        : minCoordinates(Vector3f(INFINITY, INFINITY, INFINITY))
        , maxCoordinates(Vector3f(-INFINITY, -INFINITY, -INFINITY))
    {}

    // Constructs a box given its minimum and maximum coordinates.
    Box(const Vector3f& minCoordinates,
        const Vector3f& maxCoordinates)
        : minCoordinates(minCoordinates)
        , maxCoordinates(maxCoordinates)
    {}

    // Checks whether the box contains a given point.
    bool contains(const Vector3f& point) const
    {
        return point >= minCoordinates && point <= maxCoordinates;
    }

    // Returns either the minimum or the maximum coordinates, by index. An index of 0 refers to the minimum
    // coordinates; 1 refers to the maximum coordinates.
    const Vector3f& coordinates(int index) const
    {
        // We use pointer arithmetic to obtain the coordinates of the correct Vector3f object. First, we cast
        // mMinCoordinates to a 16-byte Vector4f. Then we add index to this base address, to locate the address of the
        // correct Vector3f. Then we cast that address to a Vector3f pointer, and dereference it.
        return *reinterpret_cast<const Vector3f*>(&reinterpret_cast<const Vector4f*>(&minCoordinates)[index]);
    }

    // Returns the center of the box.
    Vector3f center() const
    {
        return (minCoordinates + maxCoordinates) * 0.5f;
    }

    // Returns a vector from the minimum coordinates to the maximum coordinates.
    Vector3f extents() const
    {
        return (maxCoordinates - minCoordinates);
    }

    // Returns the surface area of the box.
    float surfaceArea() const
    {
        // If the extents of the box are [dx dy dz], then the surface area is given by 2(dxdy + dydz + dzdx).
        return (2.0f * (extents().x() * extents().y() + extents().y() * extents().z() + extents().z() *
                        extents().x()));
    }
};

}
