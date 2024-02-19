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

// Tests based on those found in the Google spherical harmonics library:
// https://github.com/google/spherical-harmonics

#include <catch.hpp>

#include <math_functions.h>
#include <sh.h>

#define EXPECT_TUPLE3_NEAR(expected, actual, tolerance) \
  { \
    REQUIRE(expected[0] == Approx(actual[0]).epsilon(tolerance)); \
    REQUIRE(expected[1] == Approx(actual[1]).epsilon(tolerance)); \
    REQUIRE(expected[2] == Approx(actual[2]).epsilon(tolerance)); \
  }

const double kEpsilon = 1e-6;
const double kHardcodedError = 1e-5;
const double kCoeffErr = 5e-2;

// Use a lower sample count than the default so the tests complete faster.
const int kTestSampleCount = 5000;

TEST_CASE("GetIndex", "[SphericalHarmonics]")
{
    // Indices are arranged from low band to high degree, and from low order
    // to high order within a band.
    REQUIRE(0 == sh::GetIndex(0, 0));
    REQUIRE(1 == sh::GetIndex(1, -1));
    REQUIRE(2 == sh::GetIndex(1, 0));
    REQUIRE(3 == sh::GetIndex(1, 1));
    REQUIRE(4 == sh::GetIndex(2, -2));
    REQUIRE(5 == sh::GetIndex(2, -1));
    REQUIRE(6 == sh::GetIndex(2, 0));
    REQUIRE(7 == sh::GetIndex(2, 1));
    REQUIRE(8 == sh::GetIndex(2, 2));
}

TEST_CASE("GetCoefficientCount", "[SphericalHarmonics]")
{
    // For up to order n SH representation, there are (n+1)^2 coefficients.
    REQUIRE(1 == sh::GetCoefficientCount(0));
    REQUIRE(9 == sh::GetCoefficientCount(2));
    REQUIRE(16 == sh::GetCoefficientCount(3));
}

TEST_CASE("ToVector", "[SphericalHarmonics]")
{
    // Compare spherical coordinates with their known direction vectors.
    EXPECT_TUPLE3_NEAR(ipl::Vector3d(1, 0, 0), sh::ToVector(0.0, ipl::Math::kPiD / 2), kEpsilon);
    EXPECT_TUPLE3_NEAR(ipl::Vector3d(0, 1, 0), sh::ToVector(ipl::Math::kPiD / 2, ipl::Math::kPiD / 2), kEpsilon);
    EXPECT_TUPLE3_NEAR(ipl::Vector3d(0, 0, 1), sh::ToVector(0.0, 0.0), kEpsilon);
    EXPECT_TUPLE3_NEAR(ipl::Vector3d(0.5, 0.5, sqrt(0.5)), sh::ToVector(ipl::Math::kPiD / 4, ipl::Math::kPiD / 4), kEpsilon);
    EXPECT_TUPLE3_NEAR(ipl::Vector3d(0.5, 0.5, -sqrt(0.5)), sh::ToVector(ipl::Math::kPiD / 4, 3 * ipl::Math::kPiD / 4), kEpsilon);
    EXPECT_TUPLE3_NEAR(ipl::Vector3d(-0.5, 0.5, -sqrt(0.5)), sh::ToVector(3 * ipl::Math::kPiD / 4, 3 * ipl::Math::kPiD / 4), kEpsilon);
    EXPECT_TUPLE3_NEAR(ipl::Vector3d(0.5, -0.5, -sqrt(0.5)), sh::ToVector(-ipl::Math::kPiD / 4, 3 * ipl::Math::kPiD / 4), kEpsilon);
}

TEST_CASE("ToSphericalCoords", "[SphericalHarmonics]")
{
    // Compare vectors with their known spherical coordinates.
    double phi, theta;
    sh::ToSphericalCoords(ipl::Vector3d(1, 0, 0), &phi, &theta);
    REQUIRE(0.0 == phi);
    REQUIRE(ipl::Math::kPiD / 2 == theta);
    sh::ToSphericalCoords(ipl::Vector3d(0, 1, 0), &phi, &theta);
    REQUIRE(ipl::Math::kPiD / 2 == phi);
    REQUIRE(ipl::Math::kPiD / 2 == theta);
    sh::ToSphericalCoords(ipl::Vector3d(0, 0, 1), &phi, &theta);
    REQUIRE(0.0 == phi);
    REQUIRE(0.0 == theta);
    sh::ToSphericalCoords(ipl::Vector3d(0.5, 0.5, sqrt(0.5)), &phi, &theta);
    REQUIRE(ipl::Math::kPiD / 4 == phi);
    REQUIRE(ipl::Math::kPiD / 4 == theta);
    sh::ToSphericalCoords(ipl::Vector3d(0.5, 0.5, -sqrt(0.5)), &phi, &theta);
    REQUIRE(ipl::Math::kPiD / 4 == phi);
    REQUIRE(3 * ipl::Math::kPiD / 4 == theta);
    sh::ToSphericalCoords(ipl::Vector3d(-0.5, 0.5, -sqrt(0.5)), &phi, &theta);
    REQUIRE(3 * ipl::Math::kPiD / 4 == phi);
    REQUIRE(3 * ipl::Math::kPiD / 4 == theta);
    sh::ToSphericalCoords(ipl::Vector3d(0.5, -0.5, -sqrt(0.5)), &phi, &theta);
    REQUIRE(-ipl::Math::kPiD / 4 == phi);
    REQUIRE(3 * ipl::Math::kPiD / 4 == theta);
}

TEST_CASE("EvalSHSlow", "[SphericalHarmonics]")
{
    // Compare the general SH implementation to the closed form functions for
    // several bands, from: http://en.wikipedia.org/wiki/Table_of_spherical_harmonics#Real_spherical_harmonics
    // It's assumed that if the implementation matches these for this subset, the
    // probability it's correct overall is high.
    //
    // Note that for all cases |m|=1 below, we negate compared to what Wikipedia
    // lists. After careful review, it seems they do not include the (-1)^m term
    // (the Condon-Shortley phase) in their calculations.
    const double phi = ipl::Math::kPiD / 4;
    const double theta = ipl::Math::kPiD / 3;
    const ipl::Vector3d d = sh::ToVector(phi, theta);

    // l = 0
    REQUIRE(0.5 * sqrt(1 / ipl::Math::kPiD) == Approx(sh::EvalSHSlow(0, 0, phi, theta)).epsilon(kEpsilon));

    // l = 1, m = -1
    REQUIRE(sqrt(3 / (4 * ipl::Math::kPiD)) * d.y() == Approx(sh::EvalSHSlow(1, -1, phi, theta)).epsilon(kEpsilon));
    // l = 1, m = 0
    REQUIRE(sqrt(3 / (4 * ipl::Math::kPiD)) * d.z() == Approx(sh::EvalSHSlow(1, 0, phi, theta)).epsilon(kEpsilon));
    // l = 1, m = 1
    REQUIRE(sqrt(3 / (4 * ipl::Math::kPiD)) * d.x() == Approx(sh::EvalSHSlow(1, 1, phi, theta)).epsilon(kEpsilon));

    // l = 2, m = -2
    REQUIRE(0.5 * sqrt(15 / ipl::Math::kPiD) * d.x() * d.y() == Approx(sh::EvalSHSlow(2, -2, phi, theta)).epsilon(kEpsilon));
    // l = 2, m = -1
    REQUIRE(0.5 * sqrt(15 / ipl::Math::kPiD) * d.y() * d.z() == Approx(sh::EvalSHSlow(2, -1, phi, theta)).epsilon(kEpsilon));
    // l = 2, m = 0
    REQUIRE(0.25 * sqrt(5 / ipl::Math::kPiD) * (-d.x() * d.x() - d.y() * d.y() + 2 * d.z() * d.z()) == Approx(sh::EvalSHSlow(2, 0, phi, theta)).epsilon(kEpsilon));
    // l = 2, m = 1
    REQUIRE(0.5 * sqrt(15 / ipl::Math::kPiD) * d.z() * d.x() == Approx(sh::EvalSHSlow(2, 1, phi, theta)).epsilon(kEpsilon));
    // l = 2, m = 2
    REQUIRE(0.25 * sqrt(15 / ipl::Math::kPiD) * (d.x() * d.x() - d.y() * d.y()) == Approx(sh::EvalSHSlow(2, 2, phi, theta)).epsilon(kEpsilon));
}

TEST_CASE("EvalSHHardcoded", "[SphericalHarmonics]")
{
    // Arbitrary coordinates
    const double phi = 0.4296;
    const double theta = 1.73234;
    const ipl::Vector3d d = sh::ToVector(phi, theta);

    for (int l = 0; l <= 4; l++)
    {
        for (int m = -l; m <= l; m++)
        {
            double expected = sh::EvalSHSlow(l, m, phi, theta);
            REQUIRE(expected == Approx(sh::EvalSH(l, m, phi, theta)).epsilon(kHardcodedError));
            REQUIRE(expected == Approx(sh::EvalSH(l, m, d)).epsilon(kHardcodedError));
        }
    }
}

TEST_CASE("ProjectFunction", "[SphericalHarmonics]")
{
    // The expected coefficients used to define the analytic spherical function
    const ipl::vector<double> coeffs = { -1.028, 0.779, -0.275, 0.601, -0.256,
        1.891, -1.658, -0.370, -0.772 };

    // Project and compare the fitted coefficients, which should be near identical
    // to the initial coefficients
    sh::SphericalFunction func = [&](double phi, double theta)
    {
        return sh::EvalSHSum(2, coeffs.data(), phi, theta);
    };
    std::unique_ptr<std::vector<double>> fitted = sh::ProjectFunction(2, func, kTestSampleCount);
    REQUIRE(fitted != nullptr);

    for (int i = 0; i < 9; i++)
    {
        REQUIRE(coeffs[i] == Approx((*fitted)[i]).epsilon(kCoeffErr));
    }
}

#if (defined(IPL_USE_MKL))
TEST_CASE("ProjectSparseSamples", "[SphericalHarmonics]")
{
    // These are the expected coefficients that define the sparse samples of
    // the underyling spherical function
    const ipl::vector<double> coeffs = { -0.591, -0.713, 0.191, 1.206, -0.587,
        -0.051, 1.543, -0.818, 1.482 };

    // Generate sparse samples
    std::vector<ipl::Vector3d> sample_dirs;
    std::vector<double> sample_vals;
    for (int t = 0; t < 6; t++)
    {
        double theta = t * ipl::Math::kPiD / 6.0;
        for (int p = 0; p < 8; p++)
        {
            double phi = p * 2.0 * ipl::Math::kPiD / 8.0;
            ipl::Vector3d dir = sh::ToVector(phi, theta);
            double value = sh::EvalSHSum(2, coeffs, phi, theta);
            sample_dirs.push_back(dir);
            sample_vals.push_back(value);
        }
    }

    // Compute the sparse fit and given that the samples were drawn from the
    // spherical basis functions this should be a pretty ideal match
    std::unique_ptr<std::vector<double>> fitted = sh::ProjectSparseSamples(
        2, sample_dirs, sample_vals);
    REQUIRE(fitted != nullptr);

    for (int i = 0; i < 9; i++)
    {
        REQUIRE(coeffs[i] == Approx((*fitted)[i]).epsilon(kCoeffErr));
    }
}
#endif

void ExpectMatrixNear(const ipl::DynamicMatrixf& expected,
    const ipl::DynamicMatrixf& actual, double tolerance) {
    REQUIRE(expected.numRows == actual.numRows);
    REQUIRE(expected.numCols == actual.numCols);

    for (auto i = 0; i < expected.numRows; i++) {
        for (auto j = 0; j < expected.numCols; j++) {
            REQUIRE(expected(i, j) == Approx(actual(j, i)).epsilon(tolerance));
        }
    }
}

TEST_CASE("ClosedFormZAxisRotation", "[SphericalHarmonics]")
{
    // The band-level rotation matrices for a rotation about the z-axis are
    // relatively simple so we can compute them closed form and make sure the
    // recursive general approach works properly.
    // This closed form comes from [1].
    auto alpha = ipl::Math::kPi / 4.0f;
    //ipl::Quaterniond rz(Eigen::AngleAxisd(alpha, ipl::Vector3d::kZAxis));
    ipl::Quaternionf rz(0.0f, 0.0f, 0.38268343236508978f, 0.92387953251128674f);

    sh::Rotation rz_sh(3);
    rz_sh.SetRotation(rz);

    // order 0
    ipl::DynamicMatrixf r0(1, 1);
    r0(0, 0) = 1.0;
    ExpectMatrixNear(r0, rz_sh.band_rotation(0), kEpsilon);

    // order 1
    ipl::DynamicMatrixf r1({
        { cosf(alpha), 0, -sinf(alpha) },
        { 0, 1, 0 },
        { sinf(alpha), 0, cosf(alpha) }
    });

    ExpectMatrixNear(r1, rz_sh.band_rotation(1), kEpsilon);

    // order 2
    ipl::DynamicMatrixf r2({
        { cosf(2 * alpha), 0, 0, 0, -sinf(2 * alpha) },
        { 0, cosf(alpha), 0, -sinf(alpha), 0 },
        { 0, 0, 1, 0, 0 },
        { 0, sinf(alpha), 0, cosf(alpha), 0 },
        { sinf(2 * alpha), 0, 0, 0, cosf(2 * alpha) }
    });

    ExpectMatrixNear(r2, rz_sh.band_rotation(2), kEpsilon);

    // order 3
    ipl::DynamicMatrixf r3({
        { cosf(3 * alpha), 0, 0, 0, 0, 0, -sinf(3 * alpha) },
        { 0, cosf(2 * alpha), 0, 0, 0, -sinf(2 * alpha), 0 },
        { 0, 0, cosf(alpha), 0, -sinf(alpha), 0, 0 },
        { 0, 0, 0, 1, 0, 0, 0 },
        { 0, 0, sinf(alpha), 0, cosf(alpha), 0, 0 },
        { 0, sinf(2 * alpha), 0, 0, 0, cosf(2 * alpha), 0 },
        { sinf(3 * alpha), 0, 0, 0, 0, 0, cosf(3 * alpha) }
    });

    ExpectMatrixNear(r3, rz_sh.band_rotation(3), kEpsilon);
}
