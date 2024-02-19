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

#include "matrix.h"
#include "vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// CoordinateSpace3<T>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents a Cartesian coordinate system in 3D, with coordinate axes and origin. The
// coordinate system is right-handed.
template <typename T>
class CoordinateSpace3
{
public:
    Vector3<T> right; // Unit vector pointing to the right of the origin, i.e., local +x.
    Vector3<T> up; // Unit vector pointing upwards from the origin, i.e., local +y.
    Vector3<T> ahead; // Unit vector pointing ahead from the origin, i.e., local -z.
    Vector3<T> origin; // Origin of the coordinate space.

    // Constructs the canonical coordinate space. The origin is at the world-space origin, right is along +x,
    // up is along +y, and ahead is along -z.
    CoordinateSpace3()
        : right(Vector3<T>(1, 0, 0))
        , up(Vector3<T>(0, 1, 0))
        , ahead(Vector3<T>(0, 0, -1))
        , origin(Vector3<T>(0, 0, 0))
    {}

    // Constructs the canonical coordinate space. The origin specified as an argument, right is along +x,
    // up is along +y, and ahead is along -z.
    CoordinateSpace3(const Vector3<T>& origin)
        : right(Vector3<T>(1, 0, 0))
        , up(Vector3<T>(0, 1, 0))
        , ahead(Vector3<T>(0, 0, -1))
        , origin(origin)
    {}

    // Constructs a coordinate space given two mutually perpendicular vectors (ahead and up), that uniquely
    // define a right-handed coordinate system.
    CoordinateSpace3(const Vector3<T>& ahead,
                     const Vector3<T>& up,
                     const Vector3<T>& origin)
        : up(up)
        , ahead(ahead)
        , origin(origin)
    {
        right = Vector3<T>::cross(ahead, up);
    }

    // Constructs a coordinate space given one vector. A single vector does not uniquely define a coordinate
    // system. We use heuristics to select one of the infinitely many possible coordinate systems that have the
    // ahead vector as one of the axes.
    //
    // This algorithm is based on the following paper:
    //
    //  Building an orthonormal basis from a unit vector
    //  J. F. Hughes, T. Moller
    //  Journal of Graphics Tools 4(4), 1999
    //  https://pdfs.semanticscholar.org/237c/66be3fe264a11f80f9ad3d2b9ac460e76edc.pdf
    CoordinateSpace3(const Vector3<T>& ahead,
                     const Vector3<T>& origin)
        : ahead(ahead)
        , origin(origin)
    {
        if (std::abs(ahead.x()) > std::abs(ahead.z()))
        {
            right = Vector3<T>::unitVector(Vector3<T>(-ahead.y(), ahead.x(), 0));
        }
        else
        {
            right = Vector3<T>::unitVector(Vector3<T>(0, -ahead.z(), ahead.y()));
        }

        up = Vector3<T>::cross(right, ahead);
    }

    // Returns a 3x3 matrix that transforms from the canonical coordinate space to this coordinate space.
    Matrix3x3<T> toRotationMatrix() const
    {
        Matrix3x3<T> out;

        out(0, 0) = right.x();
        out(0, 1) = right.y();
        out(0, 2) = right.z();
        out(1, 0) = up.x();
        out(1, 1) = up.y();
        out(1, 2) = up.z();
        out(2, 0) = -ahead.x();
        out(2, 1) = -ahead.y();
        out(2, 2) = -ahead.z();

        return out;
    }

    // Transforms a direction from world space (the canonical coordinate space) to this coordinate space.
    Vector3<T> transformDirectionFromWorldToLocal(const Vector3<T>& direction) const
    {
        return Vector3<T>(Vector3<T>::dot(direction, right), Vector3<T>::dot(direction, up),
                          -Vector3<T>::dot(direction, ahead));
    }

    // Transforms a direction from this coordinate space to world space.
    Vector3<T> transformDirectionFromLocalToWorld(const Vector3<T>& direction) const
    {
        return direction.x() * right + direction.y() * up - direction.z() * ahead;
    }
};

typedef CoordinateSpace3<float> CoordinateSpace3f;
typedef CoordinateSpace3<double> CoordinateSpace3d;

}
