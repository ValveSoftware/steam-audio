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
// SphericalVector3<T>
// --------------------------------------------------------------------------------------------------------------------

// Represents a point in 3D space using spherical polar coordinates. Elevation is measured in the range
// [-Pi/2, Pi/2] from the horizontal, and azimuth is measured in the range [0, 2Pi] from straight ahead, going
// counter-clockwise.
template <typename T>
class SphericalVector3
{
public:
    T radius; // The radius, i.e., the distance of the point from the origin.
    T elevation; // The elevation angle.
    T azimuth; // The azimuth angle.

    // The default constructor sets the point to be at the origin.
    SphericalVector3()
        : radius(0)
        , elevation(0)
        , azimuth(0)
    {}

    // Constructs a point given its spherical coordinates.
    SphericalVector3(const T& radius,
                     const T& elevation,
                     const T& azimuth)
        : radius(radius)
        , elevation(elevation)
        , azimuth(azimuth)
    {}

    // Constructs a point by converting from Cartesian to spherical coordinates.
    SphericalVector3(const Vector3<T>& cartesian)
    {
        radius = cartesian.length();
        elevation = asin(cartesian.y() / radius);

        if (abs(elevation - Math::kHalfPi) < 1e-5f || abs(elevation + Math::kHalfPi) < 1e-5f)
        {
            azimuth = 0;
        }
        else
        {
            azimuth = fmodf(Math::kPi + atan2(cartesian.x(), cartesian.z()), 2 * Math::kPi);
        }
    }

    // Returns this point in Cartesian coordinates.
    Vector3<T> toCartesian() const
    {
        Vector3<T> out;

        out.x() = radius * cos(elevation) * -sin(azimuth);
        out.y() = radius * sin(elevation);
        out.z() = radius * cos(elevation) * -cos(azimuth);

        return out;
    }
};

typedef SphericalVector3<float> SphericalVector3f;
typedef SphericalVector3<double> SphericalVector3d;


// --------------------------------------------------------------------------------------------------------------------
// InterauralSphericalVector3<T>
// --------------------------------------------------------------------------------------------------------------------

// Represents a point in 3D space using interaural polar coordinates. Azimuth is measured in the range
// [-Pi/2, Pi/2] from straight ahead, and elevation is measured in the range [0, 2Pi] from downwards.
template <typename T>
class InterauralSphericalVector3
{
public:
    T radius; // The radius, i.e., the distance of the point from the origin.
    T azimuth; // The azimuth angle.
    T elevation; // The elevation angle.

    // The default constructor sets the point to be at the origin.
    InterauralSphericalVector3()
        : radius(0)
        , azimuth(0)
        , elevation(0)
    {}

    // Constructs a point given its interaural spherical coordinates.
    InterauralSphericalVector3(const T& radius,
                               const T& azimuth,
                               const T& elevation)
        : radius(radius)
        , azimuth(azimuth)
        , elevation(elevation)
    {}

    // Constructs a point given its spherical coordinates.
    InterauralSphericalVector3(const SphericalVector3<T>& spherical)
        : InterauralSphericalVector3(spherical.toCartesian())
    {}

    // Constructs a point by converting from Cartesian to interaural spherical coordinates.
    InterauralSphericalVector3(const Vector3<T>& cartesian)
    {
        radius = cartesian.length();
        azimuth = asinf(cartesian.x() / radius);

        if (fabsf(azimuth - Math::kHalfPi) < 1e-5f || fabsf(azimuth + Math::kHalfPi) < 1e-5f)
        {
            elevation = 0;
        }
        else
        {
            elevation = fmodf(Math::kPi + atan2(cartesian.z() / radius, cartesian.y() / radius), 2 * Math::kPi);
        }
    }

    // Returns this point in canonical spherical coordinates.
    SphericalVector3<T> toSpherical() const
    {
        return SphericalVector3<T>(toCartesian());
    }

    // Returns this point in Cartesian coordinates.
    Vector3<T> toCartesian() const
    {
        auto x = radius * sin(azimuth);
        auto y = radius * cos(azimuth) * -cos(elevation);
        auto z = radius * cos(azimuth) * -sin(elevation);
        return Vector3<T>(x, y, z);
    }
};

typedef InterauralSphericalVector3<float> InterauralSphericalVector3f;
typedef InterauralSphericalVector3<double> InterauralSphericalVector3d;


// --------------------------------------------------------------------------------------------------------------------
// CylindricalVector3<T>
// --------------------------------------------------------------------------------------------------------------------

// Represents a point in 3D space using cylindrical polar coordinates. Height is measured from the horizontal,
// and azimuth is measured in the range [0, 2Pi] from straight ahead, going counter-clockwise.
template <typename T>
class CylindricalVector3
{
public:
    T radius; // The radius, i.e., the distance of the point from the vertical axis.
    T height; // The height.
    T azimuth; // The azimuth angle.

    // The default constructor sets the point to be at the origin.
    CylindricalVector3()
        : radius(0)
        , height(0)
        , azimuth(0)
    {}

    // Constructs a point given its cylindrical coordinates.
    CylindricalVector3(const T& radius,
                       const T& height,
                       const T& azimuth)
        : radius(radius)
        , height(height)
        , azimuth(azimuth)
    {}

    // Constructs a point given its Cartesian coordinates.
    CylindricalVector3(const Vector3<T>& cartesian)
    {
        radius = sqrt(cartesian.x() * cartesian.x() + cartesian.z() * cartesian.z());
        height = cartesian.y();

        if (abs(radius) < 1e-5f)
        {
            azimuth = 0;
        }
        else
        {
            azimuth = fmodf(Math::kPi + atan2(cartesian.x(), cartesian.z()), 2 * Math::kPi);
        }
    }

    // Returns this point in Cartesian coordinates.
    Vector3<T> toCartesian() const
    {
        return Vector3<T>(radius * -sin(azimuth), height, radius * -cos(azimuth));
    }
};

typedef CylindricalVector3<float> CylindricalVector3f;
typedef CylindricalVector3<double> CylindricalVector3d;

}
