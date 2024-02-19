//
// Based on the Google spherical harmonics library: 
// https://github.com/google/spherical-harmonics
//
// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include "../array.h"
#include "../containers.h"
#include "../coordinate_space.h"
#include "../matrix.h"
#include "../quaternion.h"

namespace sh
{
    // A spherical function, the first argument is phi, the second is theta.
    // See EvalSH(int, int, double, double) for a description of these terms.
    typedef std::function<double(double, double)> SphericalFunction;

    const int kDefaultSampleCount = 10000;

    // Get the total number of coefficients for a function represented by
    // all spherical harmonic basis of degree <= @order (it is a point of
    // confusion that the order of an SH refers to its degree and not the order).
    inline int GetCoefficientCount(const int order)
    {
        return (order + 1) * (order + 1);
    }

    // Get the one dimensional index associated with a particular degree @l
    // and order @m. This is the index that can be used to access the Coeffs
    // returned by SHSolver.
    inline int GetIndex(const int l, const int m)
    {
        return l * (l + 1) + m;
    }

    // Convert from spherical coordinates to a direction vector. @phi represents
    // the rotation about the Z axis and is from [0, 2pi]. @theta represents the
    // angle down from the Z axis, from [0, pi].
    ipl::Vector3d ToVector(const double phi, const double theta);

    // Convert from a direction vector to its spherical coordinates. The
    // coordinates are written out to @phi and @theta. This is the inverse of
    // ToVector.
    // Check will fail if @dir is not unit.
    void ToSphericalCoords(const ipl::Vector3d& dir, double* phi, double* theta);

    // Evaluate the spherical harmonic basis function of degree @l and order @m
    // for the given spherical coordinates, @phi and @theta.
    // For low values of @l this will use a hard-coded function, otherwise it
    // will fallback to EvalSHSlow that uses a recurrence relation to support all l.
    double EvalSH(const int l, const int m, const double phi, const double theta);

    // Evaluate the spherical harmonic basis function of degree @l and order @m
    // for the given direction vector, @dir.
    // Check will fail if @dir is not unit.
    // For low values of @l this will use a hard-coded function, otherwise it
    // will fallback to EvalSHSlow that uses a recurrence relation to support all l.
    double EvalSH(const int l, const int m, const ipl::Vector3d& dir);

    // As EvalSH, but always uses the recurrence relationship. This is exposed
    // primarily for testing purposes to ensure the hard-coded functions equal the
    // recurrence relation version.
    double EvalSHSlow(const int l, const int m, const double phi, const double theta);

    // As EvalSH, but always uses the recurrence relationship. This is exposed
    // primarily for testing purposes to ensure the hard-coded functions equal the
    // recurrence relation version.
    // Check will fail if @dir is not unit.
    double EvalSHSlow(const int l, const int m, const ipl::Vector3d& dir);

    // Fit the given analytical spherical function to the SH basis functions
    // up to @order. This uses Monte Carlo sampling to estimate the underlying
    // integral. @sample_count determines the number of function evaluations
    // performed. @sample_count is rounded to the greatest perfect square that
    // is less than or equal to it.
    //
    // The samples are distributed uniformly over the surface of a sphere. The
    // number of samples required to get a reasonable sampling of @func depends on
    // the frequencies within that function. Lower frequency will not require as
    // many samples. The recommended default kDefaultSampleCount should be
    // sufficiently high for most functions, but is also likely overly conservative
    // for many applications.
    std::unique_ptr<std::vector<double>> ProjectFunction(const int order, 
        const SphericalFunction& func, const int sample_count);

    // Variant of ProjectFunction with user-provided output coefficients vector.
    void ProjectFunction(const int order, const SphericalFunction& func, 
        const int sample_count, double* coeffs);

    // Variant of ProjectFunction with user-provided samples and output coefficients.
    void ProjectFunction(const int order, const int sample_count, 
        const ipl::Vector3f* samples, const float* sampleValues, float* coefficients);

#if (defined(IPL_USE_MKL))
    // Fit the given samples of a spherical function to the SH basis functions
    // up to @order. This variant is used when there are relatively sparse
    // evaluations or samples of the spherical function that must be fit and a
    // regression is performed.
    // @dirs and @values must have the same size. The directions in @dirs are
    // assumed to be unit.
    std::unique_ptr<std::vector<double>> ProjectSparseSamples(const int order,
        const std::vector<Vector3d>& dirs, const std::vector<double>& values);

    // Variant of ProjectSparseSamples with user-provided output coefficients vector.
    void ProjectSparseSamples(const int order, const std::vector<Vector3d>& dirs, 
        const std::vector<double>& values, std::vector<double>& coeffs);
#endif

    // Evaluate the already computed coefficients for the SH basis functions up
    // to @order, at the coordinates @phi and @theta. The length of the @coeffs
    // vector must be equal to GetCoefficientCount(order).
    // There are explicit instantiations for double, float, and Eigen::Array3f.
    template <typename T>
    T EvalSHSum(const int order, const T* coeffs, const double phi, const double theta);

    // As EvalSHSum, but inputting a direction vector instead of spherical coords.
    // Check will fail if @dir is not unit.
    template <typename T>
    T EvalSHSum(const int order, const T* coeffs, const ipl::Vector3d& dir);


    class Rotation 
    {
        
    public:
        
        // Create a new Rotation that can applies @rotation to sets of coefficients
        // for the given @order. @order must be at least 0.
        Rotation(int order);

        // Create a new Rotation that applies the same rotation as @rotation. This
        // can be used to efficiently calculate the matrices for the same 3x3
        // transform when a new order is necessary.
        Rotation(int order, const Rotation& rotation);

        void SetRotation(const ipl::Quaternionf& quaternion);
        void SetRotation(const ipl::CoordinateSpace3f& orientation);
        void SetRotation(const ipl::Matrix3x3f& rotation);

        // Transform the SH basis coefficients in @coeff by this rotation and store
        // them into @result. These may be the same vector. The @result vector will
        // be resized if necessary, but @coeffs must have its size equal to
        // GetCoefficientCount(order()).
        //
        // This rotation transformation produces a set of coefficients that are equal
        // to the coefficients found by projecting the original function rotated by
        // the same rotation matrix.
        //
        // There are explicit instantiations for double, float, and Array3f.
        void Apply(const int order, const float* coeffs, float* result) const;
        void Apply(const int order);

        // The order (0-based) that the rotation was constructed with. It can only
        // transform coefficient vectors that were fit using the same order.
        int order() const;

        // Return the rotation that is effectively applied to the inputs of the
        // original function.
        ipl::Quaternionf rotation() const;

        // Return the (2l+1)x(2l+1) matrix for transforming the coefficients within
        // band @l by the rotation. @l must be at least 0 and less than or equal to
        // the order this rotation was initially constructed with.
        const ipl::DynamicMatrixf& band_rotation(int l) const;

        float& coefficient(const int l, const int m);
        float& rotatedCoefficient(const int l, const int m);

    private:

        const int order_;
        const ipl::Quaternionf rotation_;

        ipl::vector<ipl::DynamicMatrixf> band_rotations_;
        ipl::vector<ipl::DynamicMatrixf> band_coefficients_;
        ipl::vector<ipl::DynamicMatrixf> band_coefficients_rotated_;
    };
}
