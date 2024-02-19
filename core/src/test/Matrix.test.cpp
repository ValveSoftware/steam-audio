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

#include <catch.hpp>

#include <matrix.h>

TEST_CASE("Matrix+Matrix adds respective elements component-wise", "[Matrix]")
{
    ipl::Matrix<int, 3, 3> testMatA({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    ipl::Matrix<int, 3, 3> testMatB({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    auto resultMat = testMatA + testMatB;

    REQUIRE(resultMat(0, 0) == 2);
    REQUIRE(resultMat(0, 1) == 4);
    REQUIRE(resultMat(0, 2) == 6);
    REQUIRE(resultMat(1, 0) == 8);
    REQUIRE(resultMat(1, 1) == 10);
    REQUIRE(resultMat(1, 2) == 12);
    REQUIRE(resultMat(2, 0) == 14);
    REQUIRE(resultMat(2, 1) == 16);
    REQUIRE(resultMat(2, 2) == 18);
}

TEST_CASE("Matrix+Scalar adds the scalar to each matrix element individually", "[Matrix]")
{
    ipl::Matrix<int, 3, 3> m({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    int scalar = 5;

    auto resultMat = m + scalar;

    REQUIRE(resultMat(0, 0) == 6);
    REQUIRE(resultMat(0, 1) == 7);
    REQUIRE(resultMat(0, 2) == 8);
    REQUIRE(resultMat(1, 0) == 9);
    REQUIRE(resultMat(1, 1) == 10);
    REQUIRE(resultMat(1, 2) == 11);
    REQUIRE(resultMat(2, 0) == 12);
    REQUIRE(resultMat(2, 1) == 13);
    REQUIRE(resultMat(2, 2) == 14);
}

TEST_CASE("Matrix-Matrix subtracts respective elements component-wise", "[Matrix]")
{
    ipl::Matrix<int, 3, 3> testMatA({
        { 2, 3,  4 },
        { 5, 6,  7 },
        { 8, 9, 10 }
    });

    ipl::Matrix<int, 3, 3> testMatB({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    auto resultMat = testMatA - testMatB;

    REQUIRE(resultMat(0, 0) == 1);
    REQUIRE(resultMat(0, 1) == 1);
    REQUIRE(resultMat(0, 2) == 1);
    REQUIRE(resultMat(1, 0) == 1);
    REQUIRE(resultMat(1, 1) == 1);
    REQUIRE(resultMat(1, 2) == 1);
    REQUIRE(resultMat(2, 0) == 1);
    REQUIRE(resultMat(2, 1) == 1);
    REQUIRE(resultMat(2, 2) == 1);
}

TEST_CASE("Matrix-Scalar subtracts the scalar from each matrix element individually", "[Matrix]")
{
    ipl::Matrix<int, 3, 3> m({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    int scalar = 5;

    auto resultMat = m - scalar;

    REQUIRE(resultMat(0, 0) == -4);
    REQUIRE(resultMat(0, 1) == -3);
    REQUIRE(resultMat(0, 2) == -2);
    REQUIRE(resultMat(1, 0) == -1);
    REQUIRE(resultMat(1, 1) == 0);
    REQUIRE(resultMat(1, 2) == 1);
    REQUIRE(resultMat(2, 0) == 2);
    REQUIRE(resultMat(2, 1) == 3);
    REQUIRE(resultMat(2, 2) == 4);
}

TEST_CASE("Matrix*Scalar multiplies each matrix element by the scalar individually", "[Matrix]")
{
    ipl::Matrix<int, 3, 3> m({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    int scalar = 5;

    auto resultMat = m * scalar;

    REQUIRE(resultMat(0, 0) == 5);
    REQUIRE(resultMat(0, 1) == 10);
    REQUIRE(resultMat(0, 2) == 15);
    REQUIRE(resultMat(1, 0) == 20);
    REQUIRE(resultMat(1, 1) == 25);
    REQUIRE(resultMat(1, 2) == 30);
    REQUIRE(resultMat(2, 0) == 35);
    REQUIRE(resultMat(2, 1) == 40);
    REQUIRE(resultMat(2, 2) == 45);
}

TEST_CASE("Matrix*Vector performs standard matrix-vector multiplication", "[Matrix]")
{
    ipl::Matrix<int, 2, 3> m({
        { 1, -1, 2 },
        { 0, -3, 1 }
    });

    ipl::Vector<int, 3> v{ 2, 1, 0 };

    auto resultVec = m * v;

    REQUIRE(resultVec[0] == 1);
    REQUIRE(resultVec[1] == -3);
}

TEST_CASE("Matrix*Vector performs standard matrix-vector multiplication for float", "[Matrix]")
{
    ipl::Matrix<float, 2, 3> m({
        { 1, -1, 2 },
        { 0, -3, 1 }
    });

    ipl::Vector<float, 3> v{ 2, 1, 0 };

    auto resultVec = m * v;

    REQUIRE(resultVec[0] == Approx(1));
    REQUIRE(resultVec[1] == Approx(-3));
}

TEST_CASE("Matrix*Vector performs standard matrix-vector multiplication for Float64", "[Matrix]")
{
    ipl::Matrix<double, 2, 3> m({
        { 1, -1, 2 },
        { 0, -3, 1 }
    });

    ipl::Vector<double, 3> v{ 2, 1, 0 };

    auto resultVec = m * v;

    REQUIRE(resultVec[0] == Approx(1));
    REQUIRE(resultVec[1] == Approx(-3));
}

TEST_CASE("Matrix*Matrix performs standard matrix-matrix multiplication for float", "[Matrix]")
{
    ipl::Matrix<float, 2, 3> testMatA({
        {  0,  4, -2 },
        { -4, -3,  0 }
    });

    ipl::Matrix<float, 3, 2> testMatB({
        { 0,  1 },
        { 1, -1 },
        { 2,  3 }
    });

    auto resultMat = testMatA * testMatB;

    REQUIRE(resultMat(0, 0) == Approx(0));
    REQUIRE(resultMat(0, 1) == Approx(-10));
    REQUIRE(resultMat(1, 0) == Approx(-3));
    REQUIRE(resultMat(1, 1) == Approx(-1));
}

TEST_CASE("Matrix*Matrix performs standard matrix-matrix multiplication for Float64", "[Matrix]")
{
    ipl::Matrix<double, 2, 3> testMatA({
        {  0,  4, -2 },
        { -4, -3,  0 }
    });

    ipl::Matrix<double, 3, 2> testMatB({
        { 0,  1 },
        { 1, -1 },
        { 2,  3 }
    });

    auto resultMat = testMatA * testMatB;

    REQUIRE(resultMat(0, 0) == Approx(0));
    REQUIRE(resultMat(0, 1) == Approx(-10));
    REQUIRE(resultMat(1, 0) == Approx(-3));
    REQUIRE(resultMat(1, 1) == Approx(-1));
}

TEST_CASE("Matrix*Matrix performs standard matrix-matrix multiplication for int", "[Matrix]")
{
    ipl::Matrix<int, 2, 3> testMatA({
        {  0,  4, -2 },
        { -4, -3,  0 }
    });

    ipl::Matrix<int, 3, 2> testMatB({
        { 0,  1 },
        { 1, -1 },
        { 2,  3 }
    });

    auto resultMat = testMatA * testMatB;

    REQUIRE(resultMat(0, 0) == 0);
    REQUIRE(resultMat(0, 1) == -10);
    REQUIRE(resultMat(1, 0) == -3);
    REQUIRE(resultMat(1, 1) == -1);
}

TEST_CASE("Matrix/Scalar divides each matrix element by the scalar individually", "[Matrix]")
{
    ipl::Matrix3x3f m({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    float scalar = 5.0f;

    auto resultMat = m / scalar;

    REQUIRE(resultMat(0, 0) == Approx(1.0f / 5.0f));
    REQUIRE(resultMat(0, 1) == Approx(2.0f / 5.0f));
    REQUIRE(resultMat(0, 2) == Approx(3.0f / 5.0f));
    REQUIRE(resultMat(1, 0) == Approx(4.0f / 5.0f));
    REQUIRE(resultMat(1, 1) == Approx(5.0f / 5.0f));
    REQUIRE(resultMat(1, 2) == Approx(6.0f / 5.0f));
    REQUIRE(resultMat(2, 0) == Approx(7.0f / 5.0f));
    REQUIRE(resultMat(2, 1) == Approx(8.0f / 5.0f));
    REQUIRE(resultMat(2, 2) == Approx(9.0f / 5.0f));
}

TEST_CASE("determinant() computes determinant correctly", "[SquareMatrix]")
{
    SECTION("2x2 Matrix")
    {
        ipl::Matrix2x2f m({
            { 3, -5 },
            { 7,  2 }
        });

        REQUIRE(ipl::determinant(m) == Approx(41));
    }

    SECTION("3x3 Matrix")
    {
        ipl::Matrix3x3f m({
            {  2, -1,   9 },
            {  7, 20, -54 },
            { -3,  2,  33 }
        });

        REQUIRE(ipl::determinant(m) == Approx(2271));
    }

    SECTION("4x4 Matrix")
    {
        ipl::Matrix4x4f m({
            { -11,  31,  3, -2 },
            {   9, -21,  4,  5 },
            { -77,   9,  3,  0 },
            {  13,  -3, -7, 36 }
        });

        REQUIRE(ipl::determinant(m) == Approx(-552424));
    }

    SECTION("4x4 Matrix")
    {
        ipl::Matrix4x4f m({
            {  1,  2,  3,  4 },
            {  5,  6,  7,  8 },
            {  9, 10, 11, 12 },
            { 13, 14, 15, 16 }
        });

        REQUIRE(ipl::determinant(m) == Approx(0));
    }
}

TEST_CASE("DynamicMatrix default constructor initializes to 0x0 with a nullptr", "[DynamicMatrix]")
{
    ipl::DynamicMatrixf m;

    REQUIRE(m.numRows == 0);
    REQUIRE(m.numCols == 0);
    REQUIRE(m.elements.data() == nullptr);
}

TEST_CASE("DynamicMatrix initializer list constructor initializes correctly", "[DynamicMatrix]")
{
    ipl::DynamicMatrix<int> testMatA({
        { 1, 2, 3 },
        { 4, 5, 6 }
    });

    REQUIRE(testMatA.numRows == 2);
    REQUIRE(testMatA.numCols == 3);
    REQUIRE(testMatA(0, 0) == 1);
    REQUIRE(testMatA(0, 1) == 2);
    REQUIRE(testMatA(0, 2) == 3);
    REQUIRE(testMatA(1, 0) == 4);
    REQUIRE(testMatA(1, 1) == 5);
    REQUIRE(testMatA(1, 2) == 6);
}

TEST_CASE("DynamicMatrix copy constructor initializes correctly", "[DynamicMatrix]")
{
    ipl::DynamicMatrix<int> testMatA({
        { 1, 2, 3 },
        { 4, 5, 6 }
    });

    auto testMatB(testMatA);

    REQUIRE(testMatB.numRows == 2);
    REQUIRE(testMatB.numCols == 3);
    REQUIRE(testMatB(0, 0) == 1);
    REQUIRE(testMatB(0, 1) == 2);
    REQUIRE(testMatB(0, 2) == 3);
    REQUIRE(testMatB(1, 0) == 4);
    REQUIRE(testMatB(1, 1) == 5);
    REQUIRE(testMatB(1, 2) == 6);
}

TEST_CASE("DynamicMatrix assignment operator correctly copies data", "[DynamicMatrix]")
{
    ipl::DynamicMatrix<int> testMatA({
        { 1, 2, 3 },
        { 4, 5, 6 }
    });

    ipl::DynamicMatrix<int> testMatB;
    testMatB.operator=(testMatA);

    REQUIRE(testMatB.numRows == 2);
    REQUIRE(testMatB.numCols == 3);
    REQUIRE(testMatB(0, 0) == 1);
    REQUIRE(testMatB(0, 1) == 2);
    REQUIRE(testMatB(0, 2) == 3);
    REQUIRE(testMatB(1, 0) == 4);
    REQUIRE(testMatB(1, 1) == 5);
    REQUIRE(testMatB(1, 2) == 6);
}

TEST_CASE("DynamicMatrix addition works correctly", "[DynamicMatrix]")
{
    ipl::DynamicMatrix<int> testMatA({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    ipl::DynamicMatrix<int> testMatB({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    ipl::DynamicMatrix<int> testMatC{ testMatA.numRows, testMatA.numCols };

    ipl::addMatrices(testMatA, testMatB, testMatC);

    REQUIRE(testMatC(0, 0) == 2);
    REQUIRE(testMatC(0, 1) == 4);
    REQUIRE(testMatC(0, 2) == 6);
    REQUIRE(testMatC(1, 0) == 8);
    REQUIRE(testMatC(1, 1) == 10);
    REQUIRE(testMatC(1, 2) == 12);
    REQUIRE(testMatC(2, 0) == 14);
    REQUIRE(testMatC(2, 1) == 16);
    REQUIRE(testMatC(2, 2) == 18);
}

TEST_CASE("DynamicMatrix subtraction works correctly", "[DynamicMatrix]")
{
    ipl::DynamicMatrix<int> testMatA({
        { 2, 3,  4 },
        { 5, 6,  7 },
        { 8, 9, 10 }
    });

    ipl::DynamicMatrix<int> testMatB({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    ipl::DynamicMatrix<int> testMatC{ testMatA.numRows, testMatA.numCols };

    ipl::subtractMatrices(testMatA, testMatB, testMatC);

    REQUIRE(testMatC(0, 0) == 1);
    REQUIRE(testMatC(0, 1) == 1);
    REQUIRE(testMatC(0, 2) == 1);
    REQUIRE(testMatC(1, 0) == 1);
    REQUIRE(testMatC(1, 1) == 1);
    REQUIRE(testMatC(1, 2) == 1);
    REQUIRE(testMatC(2, 0) == 1);
    REQUIRE(testMatC(2, 1) == 1);
    REQUIRE(testMatC(2, 2) == 1);
}

TEST_CASE("DynamicMatrix scaling works correctly", "[DynamicMatrix]")
{
    ipl::DynamicMatrix<int> m({
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 }
    });

    int scalar = 2;

    ipl::DynamicMatrix<int> sm{ m.numRows, m.numCols };

    ipl::scaleMatrix(m, scalar, sm);

    REQUIRE(sm(0, 0) == 2);
    REQUIRE(sm(0, 1) == 4);
    REQUIRE(sm(0, 2) == 6);
    REQUIRE(sm(1, 0) == 8);
    REQUIRE(sm(1, 1) == 10);
    REQUIRE(sm(1, 2) == 12);
    REQUIRE(sm(2, 0) == 14);
    REQUIRE(sm(2, 1) == 16);
    REQUIRE(sm(2, 2) == 18);
}

TEST_CASE("DynamicMatrix * DynamicMatrix performs standard matrix-matrix multiplication for int", "[DynamicMatrix]")
{
    ipl::DynamicMatrix<int> testMatA({
        { 0,  4, -2 },
        { -4, -3,  0 }
    });

    ipl::DynamicMatrix<int> testMatB({
        { 0,  1 },
        { 1, -1 },
        { 2,  3 }
    });

    ipl::DynamicMatrix<int> testMatC{ testMatA.numRows, testMatB.numCols };

    ipl::multiplyMatrices(testMatA, testMatB, testMatC);

    REQUIRE(testMatC(0, 0) == 0);
    REQUIRE(testMatC(0, 1) == -10);
    REQUIRE(testMatC(1, 0) == -3);
    REQUIRE(testMatC(1, 1) == -1);
}

TEST_CASE("DynamicMatrix * DynamicMatrix performs standard matrix-matrix multiplication for float", "[DynamicMatrix]")
{
    ipl::DynamicMatrixf testMatA({
        {  0,  4, -2 },
        { -4, -3,  0 }
    });

    ipl::DynamicMatrixf testMatB({
        { 0,  1 },
        { 1, -1 },
        { 2,  3 }
    });

    ipl::DynamicMatrixf resultMat{ testMatA.numRows, testMatB.numCols };
    ipl::multiplyMatrices(testMatA, testMatB, resultMat);

    REQUIRE(resultMat(0, 0) == Approx(0));
    REQUIRE(resultMat(0, 1) == Approx(-10));
    REQUIRE(resultMat(1, 0) == Approx(-3));
    REQUIRE(resultMat(1, 1) == Approx(-1));
}

TEST_CASE("DynamicMatrix * DynamicMatrix performs standard matrix-matrix multiplication for Float64", "[DynamicMatrix]")
{
    ipl::DynamicMatrixd testMatA({
        { 0,  4, -2 },
        { -4, -3,  0 }
    });

    ipl::DynamicMatrixd testMatB({
        { 0,  1 },
        { 1, -1 },
        { 2,  3 }
    });

    ipl::DynamicMatrixd resultMat{ testMatA.numRows, testMatB.numCols };
    ipl::multiplyMatrices(testMatA, testMatB, resultMat);

    REQUIRE(resultMat(0, 0) == Approx(0));
    REQUIRE(resultMat(0, 1) == Approx(-10));
    REQUIRE(resultMat(1, 0) == Approx(-3));
    REQUIRE(resultMat(1, 1) == Approx(-1));
}

#if (defined(IPL_USE_MKL))
TEST_CASE("leastSquares solution for float", "[DynamicMatrix]")
{
    ipl::DynamicMatrixf A({
        { 0.68f,   0.597f },
        { -0.211f,  0.823f },
        { 0.566f, -0.605f }
    });

    ipl::DynamicMatrixf b({
        { -0.33f },
        { 0.536f },
        { -0.444f }
    });

    ipl::DynamicMatrixf x{ 2, 1 };

    ipl::leastSquares(A, b, x);

    REQUIRE(x(0, 0) == Approx(-0.669988453f));
    REQUIRE(x(1, 0) == Approx(0.313593656f));
}

TEST_CASE("leastSquares solution for Float64", "[DynamicMatrix]")
{
    ipl::DynamicMatrixd A({
        { 0.68,   0.597 },
        { -0.211,  0.823 },
        { 0.566, -0.605 }
    });

    ipl::DynamicMatrixd b({
        { -0.33 },
        { 0.536 },
        { -0.444 }
    });

    ipl::DynamicMatrixd x{ 2, 1 };

    ipl::leastSquares(A, b, x);

    REQUIRE(x.numRows == 2);
    REQUIRE(x.numCols == 1);
    REQUIRE(x(0, 0) == Approx(-0.669988453));
    REQUIRE(x(1, 0) == Approx(0.313593656));
}
#endif

TEST_CASE("Matrix-vector multiplication", "[Matrix]")
{
    ipl::Matrix<float, 3, 3> m{ { 1, -1, 2 },{ 0, -3, 1 },{ 0, 0, 0 } };
    ipl::Vector<float, 3> v1{ 2, 1, 0 };
    ipl::Vector<float, 3> v2{ 0, 0, 0 };

    auto vOut = m * (v1 - v2);

    REQUIRE(vOut[0] == Approx(1));
    REQUIRE(vOut[1] == Approx(-3));
}

TEST_CASE("Matrix inversion", "[Matrix]")
{
    SECTION("Case 1")
    {
        ipl::Matrix4x4f m({ {1, 1, 1, -1}, {1, 1, -1, 1}, {1, -1, 1, 1}, {-1, 1, 1, 1} });
        ipl::Matrix4x4f mInv;

        ipl::inverse(m, mInv);

        REQUIRE(mInv(0, 0) == Approx(0.25));
        REQUIRE(mInv(0, 1) == Approx(0.25));
        REQUIRE(mInv(0, 2) == Approx(0.25));
        REQUIRE(mInv(0, 3) == Approx(-0.25));
        REQUIRE(mInv(1, 0) == Approx(0.25));
        REQUIRE(mInv(1, 1) == Approx(0.25));
        REQUIRE(mInv(1, 2) == Approx(-0.25));
        REQUIRE(mInv(1, 3) == Approx(0.25));
        REQUIRE(mInv(2, 0) == Approx(0.25));
        REQUIRE(mInv(2, 1) == Approx(-0.25));
        REQUIRE(mInv(2, 2) == Approx(0.25));
        REQUIRE(mInv(2, 3) == Approx(0.25));
        REQUIRE(mInv(3, 0) == Approx(-0.25));
        REQUIRE(mInv(3, 1) == Approx(0.25));
        REQUIRE(mInv(3, 2) == Approx(0.25));
        REQUIRE(mInv(3, 3) == Approx(0.25));
    }

    SECTION("Case 2")
    {
        ipl::Matrix4x4f m({ {1, 2, 3, 4}, {3, 0, 9, 5}, {2, 0, 0, 1}, {7, 4, 1, 2} });
        ipl::Matrix4x4f mInv;

        ipl::inverse(m, mInv);

        REQUIRE(mInv(0, 0) == Approx(-0.15254237288135594));
        REQUIRE(mInv(0, 1) == Approx(0.0423728813559322));
        REQUIRE(mInv(0, 2) == Approx(0.2457627118644068));
        REQUIRE(mInv(0, 3) == Approx(0.07627118644067797));
        REQUIRE(mInv(1, 0) == Approx(0.1440677966101695));
        REQUIRE(mInv(1, 1) == Approx(-0.06779661016949153));
        REQUIRE(mInv(1, 2) == Approx(-0.5932203389830508));
        REQUIRE(mInv(1, 3) == Approx(0.17796610169491525));
        REQUIRE(mInv(2, 0) == Approx(-0.11864406779661017));
        REQUIRE(mInv(2, 1) == Approx(0.1440677966101695));
        REQUIRE(mInv(2, 2) == Approx(-0.3644067796610169));
        REQUIRE(mInv(2, 3) == Approx(0.059322033898305086));
        REQUIRE(mInv(3, 0) == Approx(0.3050847457627119));
        REQUIRE(mInv(3, 1) == Approx(-0.0847457627118644));
        REQUIRE(mInv(3, 2) == Approx(0.5084745762711864));
        REQUIRE(mInv(3, 3) == Approx(-0.15254237288135594));
    }
}
