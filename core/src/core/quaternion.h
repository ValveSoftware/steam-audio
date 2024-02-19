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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Quaternion<T>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents a quaternion.
template <typename T>
class Quaternion
{
public:
    T x; // x coordinate of the quaternion.
    T y; // y coordinate of the quaternion.
    T z; // z coordinate of the quaternion.
    T w; // Scalar component of the quaternion.

    // Constructs a quaternion with all components set to zero.
    Quaternion()
        : x(0)
        , y(0)
        , z(0)
        , w(0)
    {}

    // Constructs a quaternion given its components.
    Quaternion(const T& x,
               const T& y,
               const T& z,
               const T& w)
        : x(x)
        , y(y)
        , z(z)
        , w(w)
    {}

    // Calculates the magnitude of the quaternion.
    T length() const
    {
        return sqrt(lengthSquared());
    }

    // Calculates the squared length of the quaternion.
    T lengthSquared() const
    {
        return x*x + y*y + z*z + w*w;
    }

    // Normalizes the quaternion. Only normalized quaternions can be used to represent rotations.
    void normalize()
    {
        auto oneOverLen = 1.0 / length();
        x *= oneOverLen;
        y *= oneOverLen;
        z *= oneOverLen;
        w *= oneOverLen;
    }

    // Converts this quaternion to a 3x3 rotation matrix. The quaternion must be normalized before calling this
    // function.
    Matrix3x3<T> toRotationMatrix() const
    {
        Matrix3x3<T> out;

        auto x2 = x + x;
        auto y2 = y + y;
        auto z2 = z + z;

        auto xSq2 = x * x2;
        auto ySq2 = y * y2;
        auto zSq2 = z * z2;

        auto xw2 = x2 * w;
        auto yw2 = y2 * w;
        auto zw2 = z2 * w;

        auto xy2 = x2 * y;
        auto xz2 = x2 * z;
        auto yz2 = y2 * z;

        out(0, 0) = 1 - ySq2 - zSq2;
        out(0, 1) = xy2 - zw2;
        out(0, 2) = xz2 + yw2;

        out(1, 0) = xy2 + zw2;
        out(1, 1) = 1 - xSq2 - zSq2;
        out(1, 2) = yz2 - xw2;

        out(2, 0) = xz2 - yw2;
        out(2, 1) = yz2 + xw2;
        out(2, 2) = 1 - xSq2 - ySq2;

        return out;
    }
};

// Multiplies (concatenates) two quaternions.
template <typename T>
Quaternion<T> operator*(const Quaternion<T>& lhs,
                        const Quaternion<T>& rhs)
{
    return Quaternion<T>(lhs.w*rhs.x + lhs.x*rhs.w + lhs.y*rhs.z - lhs.z*rhs.y,
                         lhs.w*rhs.y + lhs.y*rhs.w + lhs.z*rhs.x - lhs.x*rhs.z,
                         lhs.w*rhs.z + lhs.z*rhs.w + lhs.x*rhs.y - lhs.y*rhs.x,
                         lhs.w*rhs.w - lhs.x*rhs.x - lhs.y*rhs.y - lhs.z*rhs.z);
}

typedef Quaternion<float> Quaternionf;
typedef Quaternion<double> Quaterniond;

}
