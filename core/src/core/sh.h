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

#include "sh/spherical_harmonics.h"

#include "array.h"
#include "containers.h"
#include "coordinate_space.h"
#include "matrix.h"
#include "quaternion.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SphericalHarmonics
// --------------------------------------------------------------------------------------------------------------------

// Wrappers for Spherical Harmonics functions that perform appropriate coordinate transforms and type conversions.
namespace SphericalHarmonics
{
    int numCoeffsForOrder(int order);

    float legendre(int n,
                   float x);

    template <typename T>
    Vector3<T> convertedDirection(const Vector3f& direction);

    Quaternionf convertedQuaternion(const Quaternionf& quaternion);

    CoordinateSpace3f convertedCoordinateSpace(const CoordinateSpace3f& coordinateSpace);

    float evaluate(int l,
                   int m,
                   const Vector3f& direction);

    float evaluateSum(int order,
                      const float* coefficients,
                      const Vector3f& direction);

    void projectSinglePoint(const Vector3f& direction,
                            int order,
                            float* coefficients);

    void projectSinglePointAndUpdate(const Vector3f& direction,
                                     int order,
                                     float gain,
                                     float* coefficients);
}


// --------------------------------------------------------------------------------------------------------------------
// SHRotation
// --------------------------------------------------------------------------------------------------------------------

class SHRotation
{
public:
    SHRotation(int order);

    void setRotation(const Quaternionf& quaternion);

    void setRotation(const CoordinateSpace3f& coordinateSpace);

    void setCoeff(int l,
                  int m,
                  float value);

    float getRotatedCoeff(int l,
                          int m) const;

    void apply(int order);

    void apply(int order,
               const float* coeffs,
               float* rotatedCoeffs) const;

private:
    sh::Rotation mRotation;
};

}