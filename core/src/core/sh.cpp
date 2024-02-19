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

#include "sh.h"

#include <random>

#include "math_functions.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SphericalHarmonics
// --------------------------------------------------------------------------------------------------------------------

int SphericalHarmonics::numCoeffsForOrder(int order)
{
    return sh::GetCoefficientCount(order);
}

float SphericalHarmonics::legendre(int n,
                                   float x)
{
    switch (n)
    {
    case 0:
        return 1.0f;
    case 1:
        return x;
    case 2:
        return 0.5f * (3.0f * x * x - 1.0f);
    case 3:
        return 0.5f * x * (5.0f * x * x - 3.0f);
    default:
        return ((2 * n + 1) * x * legendre(n, x) - n * legendre(n - 1, x)) / (n + 1);
    }
}

template <typename T>
Vector3<T> SphericalHarmonics::convertedDirection(const Vector3f& direction)
{
    // The Google SH library uses a coordinate system that is +x forward, +y left,
    // and +z up. We use a coordinate system that is +x right, +y up, and +z backward.
    return Vector3<T>(-direction.z(), -direction.x(), direction.y());
}

template Vector3<float> SphericalHarmonics::convertedDirection(const Vector3f& direction);

template Vector3<double> SphericalHarmonics::convertedDirection(const Vector3f& direction);

Quaternionf SphericalHarmonics::convertedQuaternion(const Quaternionf& quaternion)
{
    return Quaternionf(0.5f, -0.5f, -0.5f, 0.5f) * quaternion;
}

CoordinateSpace3f SphericalHarmonics::convertedCoordinateSpace(const CoordinateSpace3f& coordinateSpace)
{
    return CoordinateSpace3f(convertedDirection<float>(coordinateSpace.ahead),
                             convertedDirection<float>(coordinateSpace.up),
                             coordinateSpace.origin);
}

float SphericalHarmonics::evaluate(int l,
                                   int m,
                                   const Vector3f& direction)
{
    return static_cast<float>(sh::EvalSH(l, m, convertedDirection<double>(direction)));
}

float SphericalHarmonics::evaluateSum(int order,
                                      const float* coefficients,
                                      const Vector3f& direction)
{
    return sh::EvalSHSum(order, coefficients, convertedDirection<double>(direction));
}

void SphericalHarmonics::projectSinglePoint(const Vector3f& direction,
                                            int order,
                                            float* coefficients)
{
    auto dir = convertedDirection<double>(direction);
    for (auto l = 0, index = 0; l <= order; ++l)
    {
        for (auto m = -l; m <= l; ++m)
        {
            coefficients[index++] = static_cast<float>(sh::EvalSH(l, m, dir));
        }
    }
}

void SphericalHarmonics::projectSinglePointAndUpdate(const Vector3f& direction,
                                                     int order,
                                                     float gain,
                                                     float* coefficients)
{
    auto dir = convertedDirection<double>(direction);
    for (auto l = 0, index = 0; l <= order; ++l)
    {
        for (auto m = -l; m <= l; ++m)
        {
            coefficients[index++] += gain * static_cast<float>(sh::EvalSH(l, m, dir));
        }
    }
}


// --------------------------------------------------------------------------------------------------------------------
// SHRotation
// --------------------------------------------------------------------------------------------------------------------

SHRotation::SHRotation(int order)
    : mRotation(order)
{}

void SHRotation::setRotation(const Quaternionf& quaternion)
{
    mRotation.SetRotation(SphericalHarmonics::convertedQuaternion(quaternion));
}

void SHRotation::setRotation(const CoordinateSpace3f& coordinateSpace)
{
    mRotation.SetRotation(SphericalHarmonics::convertedCoordinateSpace(coordinateSpace));
}

void SHRotation::setCoeff(int l,
                          int m,
                          float value)
{
    mRotation.coefficient(l, m) = value;
}

float SHRotation::getRotatedCoeff(int l,
                                  int m) const
{
    return const_cast<sh::Rotation&>(mRotation).rotatedCoefficient(l, m);
}

void SHRotation::apply(int order)
{
    const_cast<sh::Rotation&>(mRotation).Apply(order);
}

void SHRotation::apply(int order,
                       const float* coeffs,
                       float* rotatedCoeffs) const
{
    mRotation.Apply(order, coeffs, rotatedCoeffs);
}

}