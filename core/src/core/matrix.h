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

#if defined(IPL_USE_MKL)
#include <mkl/mkl_blas.h>
#include <mkl/mkl_cblas.h>
#include <mkl/mkl_lapack.h>
#include <mkl/mkl_lapacke.h>
#endif

#include "containers.h"
#include "vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Matrix<T, R, C>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents a stack-allocated of matrix of arbitrary dimensions.
template <typename T, const int R, const int C>
class Matrix
{
public:
    static const int kNumRows = R; // Number of rows.
    static const int kNumCols = C; // Number of columns.
    static const int kNumElements = R * C; // Total number of elements.

    T elements[R * C]; // Elements of this matrix, stored in column-major order.

    // Default constructor. Does not initialize matrix elements.
    Matrix()
    {}

    // Copies matrix entries from @mat. @mat is required to have R lists, each of size C.
    Matrix(std::initializer_list<std::initializer_list<T>> mat)
    {
        assert(mat.size() == R);

        auto i = 0, j = 0;
        for (const auto& row : mat)
        {
            assert(row.size() == C);
            for (const auto& entry : row)
            {
                (*this)(i, j) = entry;
                ++j;
            }
            j = 0;
            ++i;
        }
    }

    const T& operator()(int row,
                        int col) const
    {
        return elements[(col * R) + row];
    }

    T& operator()(int row,
                  int col)
    {
        return elements[(col * R) + row];
    }

    // Adds another matrix element-wise into this matrix.
    Matrix<T, R, C>& operator+=(const Matrix<T, R, C>& rhs)
    {
        for (auto i = 0; i < kNumElements; ++i)
        {
            elements[i] += rhs.elements[i];
        }

        return *this;
    }

    // Adds a constant to each element of this matrix.
    Matrix<T, R, C>& operator+=(const T& s)
    {
        for (auto i = 0; i < kNumElements; ++i)
        {
            elements[i] += s;
        }

        return *this;
    }

    // Subtracts another matrix element-wise from this matrix.
    Matrix<T, R, C>& operator-=(const Matrix<T, R, C>& rhs)
    {
        for (auto i = 0; i < kNumElements; ++i)
        {
            elements[i] -= rhs.elements[i];
        }

        return *this;
    }

    // Subtracts a constant from each element of this matrix.
    Matrix<T, R, C>& operator-=(const T& s)
    {
        for (auto i = 0; i < kNumElements; ++i)
        {
            elements[i] -= s;
        }

        return *this;
    }

    // Multiplies each element of this matrix by a constant.
    Matrix<T, R, C>& operator*=(const T& s)
    {
        for (auto i = 0; i < kNumElements; ++i)
        {
            elements[i] *= s;
        }

        return *this;
    }

    // Divides each element of this matrix by a constant.
    Matrix<T, R, C>& operator/=(const T& s)
    {
        assert(s != 0);

        for (auto i = 0; i < kNumElements; ++i)
        {
            elements[i] *= static_cast<T>(1) / s;
        }

        return *this;
    }

    // Returns a matrix that is the transpose of this matrix.
    Matrix<T, C, R> transposedCopy() const
    {
        Matrix<T, C, R> out;

        for (auto i = 0; i < R; ++i)
        {
            for (auto j = 0; j < C; ++j)
            {
                out(i, j) = (*this)(j, i);
            }
        }

        return out;
    }

    // Sets all elements of this matrix to zero.
    void zero()
    {
        memset(elements, 0, kNumElements * sizeof(T));
    }

    // Returns the zero matrix.
    static Matrix<T, R, C> zeroMatrix()
    {
        Matrix<T, R, C> out;
        out.zero();
        return out;
    }
};

// Adds two matrices.
template <typename T, const int R, const int C>
Matrix<T, R, C> operator+(const Matrix<T, R, C>& lhs,
                          const Matrix<T, R, C>& rhs)
{
    Matrix<T, R, C> out(lhs);
    out += rhs;
    return out;
}

// Adds a constant to each element of a matrix.
template <typename T, const int R, const int C>
Matrix<T, R, C> operator+(const Matrix<T, R, C>& m,
                          const T& s)
{
    Matrix<T, R, C> out(m);
    out += s;
    return out;
}

// Subtracts one matrix from another.
template <typename T, const int R, const int C>
Matrix<T, R, C> operator-(const Matrix<T, R, C>& lhs,
                          const Matrix<T, R, C>& rhs)
{
    Matrix<T, R, C> out(lhs);
    out -= rhs;
    return out;
}

// Subtracts a constant from every element of a matrix.
template <typename T, const int R, const int C>
Matrix<T, R, C> operator-(const Matrix<T, R, C>& m,
                          const T& s)
{
    Matrix<T, R, C> out(m);
    out -= s;
    return out;
}

// Multiplies each element of a matrix by a constant.
template <typename T, const int R, const int C>
Matrix<T, R, C> operator*(const Matrix<T, R, C>& m,
                          const T& s)
{
    Matrix<T, R, C> out(m);
    out *= s;
    return out;
}

// Calculates a matrix-vector product.
template <typename T, const int R, const int C>
Vector<T, R> operator*(const Matrix<T, R, C>& m,
                       const Vector<T, C>& v)
{
    Vector<T, R> out = Vector<T, R>::kZero;

    for (auto i = 0; i < R; ++i)
    {
        for (auto j = 0; j < C; ++j)
        {
            out[i] += m(i, j) * v[j];
        }
    }

    return out;
}

// Multiplies two matrices.
template <typename T, const int R, const int C, const int C2>
Matrix<T, R, C2> operator*(const Matrix<T, R, C>& lhs,
                           const Matrix<T, C, C2>& rhs)
{
    Matrix<T, R, C2> out;

    out.zero();
    for (auto i = 0; i < R; ++i)
    {
        for (auto j = 0; j < C2; ++j)
        {
            for (auto k = 0; k < C; ++k)
            {
                out(i, j) += lhs(i, k) * rhs(k, j);
            }
        }
    }

    return out;
}

// Divides each element of a matrix by a constant.
template <typename T, const int R, const int C>
Matrix<T, R, C> operator/(const Matrix<T, R, C>& m,
                          const T& s)
{
    assert(s != 0);
    Matrix<T, R, C> out(m);
    out /= s;
    return out;
}


// --------------------------------------------------------------------------------------------------------------------
// SquareMatrix<T, N>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents a stack-allocated square matrix.
template <typename T, const int N>
class SquareMatrix : public Matrix<T, N, N>
{
public:
    // Default constructor. Does not initialize matrix elements.
    SquareMatrix()
    {}

    // Copy constructs a square matrix from another square matrix.
    SquareMatrix(const Matrix<T, N, N>& values)
        : Matrix<T, N, N>(values)
    {}

    // Calculates the transpose of a square matrix.
    SquareMatrix<T, N>& transpose()
    {
        for (auto i = 0; i < N; ++i)
        {
            for (auto j = 0; j < N; ++j)
            {
                std::swap((*this)(i, j), (*this)(j, i));
            }
        }

        return *this;
    }

    // Sets this matrix to be the identity matrix.
    void identity()
    {
        this->zero();
        for (auto i = 0; i < N; ++i)
        {
            (*this)(i, i) = static_cast<T>(1);
        }
    }

    // Returns the identity matrix.
    static SquareMatrix<T, N> identityMatrix()
    {
        SquareMatrix<T, N> out;
        out.identity();
        return out;
    }
};

// Calculates the determinant of a 2x2 matrix.
template <typename T>
T determinant(const SquareMatrix<T, 2>& m)
{
    return m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0);
}

// Calculates the determinant of a 3x3 matrix.
//
// Given:
//
//     | a b c |
// m = | d e f |
//     | g h i |
//
// We compute:
//
//          | e f |     | d f |     | d e |
// |m| =  a | h i | - b | g i | + c | g h |
template <typename T>
T determinant(const SquareMatrix<T, 3>& m)
{
    auto minor_efhi = m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1);
    auto minor_dfgi = m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0);
    auto minor_degh = m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0);

    return m(0, 0) * minor_efhi - m(0, 1) * minor_dfgi + m(0, 2) * minor_degh;
}

// Calculates the determinant of a 4x4 matrix.
//
// Given:
//
//     | a b c d |
// m = | e f g h |
//     | i j k l |
//     | m n o p |
//
// We compute:
//
//         | f g h |     | e g h |     | e f h |     | e f g |
// |m| = a | j k l | - b | i k l | + c | i j l | - d | i j k |
//         | n o p |     | m o p |     | m n p |     | m n o |
//
// | f g h |
// | j k l | = f | k l | - g | j l | + h | j k |
// | n o p |     | o p |     | n p |     | n o |
//
// | e g h |
// | i k l | = e | k l | - g | i l | + h | i k |
// | m o p |     | o p |     | m p |     | m o |
//
// | e f h |
// | i j l | = e | j l | - f | i l | + h | i j |
// | m n p |     | n p |     | m p |     | m n |
//
// | e f g |
// | i j k | = e | j k | - f | i k | + g | i j |
// | m n o |     | n o |     | m o |     | m n |
template <typename T>
T determinant(const SquareMatrix<T, 4>& m)
{
    // 2x2 minors
    auto minor_klop = m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2);
    auto minor_jlnp = m(2, 1) * m(3, 3) - m(3, 1) * m(2, 3);
    auto minor_jkno = m(2, 1) * m(3, 2) - m(2, 2) * m(3, 1);

    //auto minor_klop = // already computed
    auto minor_ilmp = m(2, 0) * m(3, 3) - m(2, 3) * m(3, 0);
    auto minor_ikmo = m(2, 0) * m(3, 2) - m(2, 2) * m(3, 0);

    //auto minor_jlnp = // already computed
    //auto minor_ilmp = // already computed
    auto minor_ijmn = m(2, 0) * m(3, 1) - m(2, 1) * m(3, 0);

    //auto minor_jkno = // already computed
    //auto minor_ikmo = // already computed
    //auto minor_ijmn = // already computed

    // 3x3 minors
    auto minor_fghjklnop = m(1, 1) * minor_klop - m(1, 2) * minor_jlnp + m(1, 3) * minor_jkno;
    auto minor_eghiklmop = m(1, 0) * minor_klop - m(1, 2) * minor_ilmp + m(1, 3) * minor_ikmo;
    auto minor_efhijlmnp = m(1, 0) * minor_jlnp - m(1, 1) * minor_ilmp + m(1, 3) * minor_ijmn;
    auto minor_efgijkmno = m(1, 0) * minor_jkno - m(1, 1) * minor_ikmo + m(1, 2) * minor_ijmn;

    return m(0, 0) * minor_fghjklnop - m(0, 1) * minor_eghiklmop + m(0, 2) * minor_efhijlmnp - m(0, 3) *
        minor_efgijkmno;
}

// Given:
//
//     | a b c d |
// m = | e f g h |
//     | i j k l |
//     | m n o p |
//
//        | f g h |
// d_11 = | j k l | = f | k l | - g | j l | + h | j k |
//        | n o p |     | o p |     | n p |     | n o |
//
//        | e g h |
// d_12 = | i k l | = e | k l | - g | i l | + h | i k |
//        | m o p |     | o p |     | m p |     | m o |
//
//        | e f h |
// d_13 = | i j l | = e | j l | - f | i l | + h | i j |
//        | m n p |     | n p |     | m p |     | m n |
//
//        | e f g |
// d_14 = | i j k | = e | j k | - f | i k | + g | i j |
//        | m n o |     | n o |     | m o |     | m n |
//
//        | b c d |
// d_21 = | j k l | = b | k l | - c | j l | + d | j k |
//        | n o p |     | o p |     | n p |     | n o |
//
//        | a c d |
// d_22 = | i k l | = a | k l | - c | i l | + d | i k |
//        | m o p |     | o p |     | m p |     | m o |
//
//        | a b d |
// d_23 = | i j l | = a | j l | - b | i l | + d | i j |
//        | m n p |     | n p |     | m p |     | m n |
//
//        | a b c |
// d_24 = | i j k | = a | j k | - b | i k | + c | i j |
//        | m n o |     | n o |     | m o |     | m n |
//
//        | b c d |
// d_31 = | f g h | = b | g h | - c | f h | + d | f g |
//        | n o p |     | o p |     | n p |     | n o |
//
//        | a c d |
// d_32 = | e g h | = a | g h | - c | e h | + d | e g |
//        | m o p |     | o p |     | m p |     | m o |
//
//        | a b d |
// d_33 = | e f h | = a | f h | - b | e h | + d | e f |
//        | m n p |     | n p |     | m p |     | m n |
//
//        | a b c |
// d_34 = | e f g | = a | f g | - b | e g | + c | e f |
//        | m n o |     | n o |     | m o |     | m n |
//
//        | b c d |
// d_41 = | f g h | = b | g h | - c | f h | + d | f g |
//        | j k l |     | k l |     | j l |     | j k |
//
//        | a c d |
// d_42 = | e g h | = a | g h | - c | e h | + d | e g |
//        | i k l |     | k l |     | i l |     | i k |
//
//        | a b d |
// d_43 = | e f h | = a | f h | - b | e h | + d | e f |
//        | i j l |     | j l |     | i l |     | i j |
//
//        | a b c |
// d_44 = | e f g | = a | f g | - b | e g | + c | e f |
//        | i j k |     | j k |     | i k |     | i j |
//
template <typename T>
void inverse(const SquareMatrix<T, 4>& m,
             SquareMatrix<T, 4>& mInv)
{
    // 2x2 minors
    auto klop = m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2);
    auto jlnp = m(2, 1) * m(3, 3) - m(2, 3) * m(3, 1);
    auto jkno = m(2, 1) * m(3, 2) - m(2, 2) * m(3, 1);
    auto ilmp = m(2, 0) * m(3, 3) - m(2, 3) * m(3, 0);
    auto ikmo = m(2, 0) * m(3, 2) - m(2, 2) * m(3, 0);
    auto ijmn = m(2, 0) * m(3, 1) - m(2, 1) * m(3, 0);
    auto ghop = m(1, 2) * m(3, 3) - m(1, 3) * m(3, 2);
    auto fhnp = m(1, 1) * m(3, 3) - m(1, 3) * m(3, 1);
    auto fgno = m(1, 1) * m(3, 2) - m(1, 2) * m(3, 1);
    auto ehmp = m(1, 0) * m(3, 3) - m(1, 3) * m(3, 0);
    auto egmo = m(1, 0) * m(3, 2) - m(1, 2) * m(3, 0);
    auto efmn = m(1, 0) * m(3, 1) - m(1, 1) * m(3, 0);
    auto ghkl = m(1, 2) * m(2, 3) - m(1, 3) * m(2, 2);
    auto fhjl = m(1, 1) * m(2, 3) - m(1, 3) * m(2, 1);
    auto fgjk = m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1);
    auto ehil = m(1, 0) * m(2, 3) - m(1, 3) * m(2, 0);
    auto egik = m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0);
    auto efij = m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0);

    // 3x3 minors
    auto d_11 = m(1, 1) * klop - m(1, 2) * jlnp + m(1, 3) * jkno;
    auto d_12 = m(1, 0) * klop - m(1, 2) * ilmp + m(1, 3) * ikmo;
    auto d_13 = m(1, 0) * jlnp - m(1, 1) * ilmp + m(1, 3) * ijmn;
    auto d_14 = m(1, 0) * jkno - m(1, 1) * ikmo + m(1, 2) * ijmn;
    auto d_21 = m(0, 1) * klop - m(0, 2) * jlnp + m(0, 3) * jkno;
    auto d_22 = m(0, 0) * klop - m(0, 2) * ilmp + m(0, 3) * ikmo;
    auto d_23 = m(0, 0) * jlnp - m(0, 1) * ilmp + m(0, 3) * ijmn;
    auto d_24 = m(0, 0) * jkno - m(0, 1) * ikmo + m(0, 2) * ijmn;
    auto d_31 = m(0, 1) * ghop - m(0, 2) * fhnp + m(0, 3) * fgno;
    auto d_32 = m(0, 0) * ghop - m(0, 2) * ehmp + m(0, 3) * egmo;
    auto d_33 = m(0, 0) * fhnp - m(0, 1) * ehmp + m(0, 3) * efmn;
    auto d_34 = m(0, 0) * fgno - m(0, 1) * egmo + m(0, 2) * efmn;
    auto d_41 = m(0, 1) * ghkl - m(0, 2) * fhjl + m(0, 3) * fgjk;
    auto d_42 = m(0, 0) * ghkl - m(0, 2) * ehil + m(0, 3) * egik;
    auto d_43 = m(0, 0) * fhjl - m(0, 1) * ehil + m(0, 3) * efij;
    auto d_44 = m(0, 0) * fgjk - m(0, 1) * egik + m(0, 2) * efij;

    // 4x4 determinant
    auto det = m(0, 0) * d_11 - m(0, 1) * d_12 + m(0, 2) * d_13 - m(0, 3) * d_14;

    // inverse
    if (det != 0)
    {
        mInv(0, 0) = d_11;
        mInv(1, 0) = -d_12;
        mInv(2, 0) = d_13;
        mInv(3, 0) = -d_14;
        mInv(0, 1) = -d_21;
        mInv(1, 1) = d_22;
        mInv(2, 1) = -d_23;
        mInv(3, 1) = d_24;
        mInv(0, 2) = d_31;
        mInv(1, 2) = -d_32;
        mInv(2, 2) = d_33;
        mInv(3, 2) = -d_34;
        mInv(0, 3) = -d_41;
        mInv(1, 3) = d_42;
        mInv(2, 3) = -d_43;
        mInv(3, 3) = d_44;
        mInv /= det;
    }
    else
    {
        mInv.zero();
    }
}

template <typename T>
using Matrix2x2 = SquareMatrix<T, 2>;

template <typename T>
using Matrix3x3 = SquareMatrix<T, 3>;

template <typename T>
using Matrix4x4 = SquareMatrix<T, 4>;

typedef Matrix2x2<float> Matrix2x2f;
typedef Matrix3x3<float> Matrix3x3f;
typedef Matrix4x4<float> Matrix4x4f;

typedef Matrix2x2<double> Matrix2x2d;
typedef Matrix3x3<double> Matrix3x3d;
typedef Matrix4x4<double> Matrix4x4d;


// --------------------------------------------------------------------------------------------------------------------
// DynamicMatrix<T>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents a heap-allocated matrix of arbitrary dimensions.
template <typename T>
class DynamicMatrix
{
public:
    vector<T> elements; // The elements, in column-major order.
    int numRows; // Number of rows.
    int numCols; // Number of columns.

    // Default constructs a 0x0 matrix. Does not allocate memory.
    DynamicMatrix()
        : numRows(0)
        , numCols(0)
    {}

    // Allocates room for a matrix with @rows rows and @cols columns. Does not initialize elements.
    DynamicMatrix(int rows,
                  int cols)
        : elements(rows * cols)
        , numRows(rows)
        , numCols(cols)
    {}

    // Allocates room for a matrix with @rows rows and @cols columns. Copies in matrix entries from @elements.
    DynamicMatrix(int rows,
                  int cols,
                  const T* elements)
        : DynamicMatrix(rows, cols)
    {
        memcpy(this->elements.data(), elements, rows * cols * sizeof(T));
    }

    // Allocates room for a matrix specified by the doubly-nested initializer list @mat. @mat is required to
    // contain lists of equal size.
    DynamicMatrix(std::initializer_list<std::initializer_list<T>> mat)
    {
        numRows = static_cast<int>(mat.size());
        numCols = numRows == 0 ? 0 : static_cast<int>(mat.begin()->size());

        elements.resize(numRows * numCols);

        if (numRows > 0 && numCols > 0)
        {
            auto i = 0;
            auto j = 0;
            for (auto row : mat)
            {
                assert(row.size() == numCols);
                for (auto entry : row)
                {
                    (*this)(i, j) = entry;
                    ++j;
                }
                j = 0;
                ++i;
            }
        }
    }

    // Accessors that return matrix elements by row and column index.
    const T& operator()(int row,
                        int col) const
    {
        assert(row < numRows && col < numCols);
        return elements[(col * numRows) + row];
    }

    T& operator()(int row,
                  int col)
    {
        assert(row < numRows && col < numCols);
        return elements[(col * numRows) + row];
    }

    // Sets all elements of this matrix to zero.
    void zero()
    {
        memset(elements.data(), 0, numRows * numCols * sizeof(T));
    }

    void resize(int numRows, int numCols)
    {
        this->numRows = numRows;
        this->numCols = numCols;

        elements.resize(numRows * numCols);

        zero();
    }
};

// Adds two matrices.
template <typename T>
void addMatrices(const DynamicMatrix<T>& in1,
                 const DynamicMatrix<T>& in2,
                 DynamicMatrix<T>& out)
{
    assert(in1.numRows == out.numRows && in2.numRows == out.numRows);
    assert(in1.numCols == out.numCols && in2.numCols == out.numCols);

    for (auto i = 0; i < out.numRows * out.numCols; ++i)
    {
        out.elements[i] = in1.elements[i] + in2.elements[i];
    }
}

// Subtracts two matrices.
template <typename T>
void subtractMatrices(const DynamicMatrix<T>& in1,
                      const DynamicMatrix<T>& in2,
                      DynamicMatrix<T>& out)
{
    assert(in1.numRows == out.numRows && in2.numRows == out.numRows);
    assert(in1.numCols == out.numCols && in2.numCols == out.numCols);

    for (auto i = 0; i < out.numRows * out.numCols; ++i)
    {
        out.elements[i] = in1.elements[i] - in2.elements[i];
    }
}

// Scales a matrix.
template <typename T>
void scaleMatrix(const DynamicMatrix<T>& in,
                 T s,
                 DynamicMatrix<T>& out)
{
    assert(in.numRows == out.numRows);
    assert(in.numCols == out.numCols);

    for (auto i = 0; i < out.numRows * out.numCols; ++i)
    {
        out.elements[i] = s * in.elements[i];
    }
}

// Multiplies two matrices.
template <typename T>
void multiplyMatrices(const DynamicMatrix<T>& a,
                      const DynamicMatrix<T>& b,
                      DynamicMatrix<T>& c)
{
    assert(a.numRows == c.numRows);
    assert(b.numCols == c.numCols);
    assert(a.numCols == b.numRows);

    c.zero();

    auto aData = a.elements.data();
    auto bData = b.elements.data();
    auto cData = c.elements.data();

    for (auto j = 0, bOffset = 0, cIndex = 0; j < c.numCols; ++j, bOffset += b.numRows)
    {
        for (auto i = 0; i < c.numRows; ++i, ++cIndex)
        {
            for (auto k = 0, aIndex = i, bIndex = bOffset; k < a.numCols; ++k, aIndex += a.numRows, ++bIndex)
            {
                cData[cIndex] += aData[aIndex] * bData[bIndex];
            }
        }
    }
}

template <typename T>
void multiplyMatrixVector(const DynamicMatrix<T>& m,
                          const T* v,
                          T* mv)
{
    for (auto i = 0; i < m.numRows; ++i)
    {
        mv[i] = 0;
        for (auto j = 0; j < m.numCols; ++j)
        {
            mv[i] += m(i, j) * v[j];
        }
    }
}

// Calculates the least squares solution of a linear system of equations, Ax = b.
template <typename T>
void leastSquares(const DynamicMatrix<T>& A,
                  const DynamicMatrix<T>& b,
                  DynamicMatrix<T>& x)
{
    throw Exception(Status::Failure);
}

typedef DynamicMatrix<float> DynamicMatrixf;
typedef DynamicMatrix<double> DynamicMatrixd;

#if defined(IPL_USE_MKL)

// Optimized matrix-matrix multiplication for float values.
template <>
void multiplyMatrices(const DynamicMatrixf& a,
                      const DynamicMatrixf& b,
                      DynamicMatrixf& c)
{
    assert(a.numRows == c.numRows);
    assert(b.numCols == c.numCols);
    assert(a.numCols == b.numRows);

    cblas_sgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, a.numRows, b.numCols, a.numCols, 1.0f, a.elements.data(),
                a.numRows, b.elements.data(), a.numCols, 0.0f, c.elements.data(), a.numRows);
}

// Optimized matrix-matrix multiplication for double values.
template <>
void multiplyMatrices(const DynamicMatrixd& a,
                      const DynamicMatrixd& b,
                      DynamicMatrixd& c)
{
    assert(a.numRows == c.numRows);
    assert(b.numCols == c.numCols);
    assert(a.numCols == b.numRows);

    cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, a.numRows, b.numCols, a.numCols, 1.0f, a.elements.data(),
                a.numRows, b.elements.data(), a.numCols, 0.0f, c.elements.data(), a.numRows);
}

// Calculates the least squares solution of a linear system of equations, for float values.
template <>
void leastSquares(const DynamicMatrixf& A,
                  const DynamicMatrixf& b,
                  DynamicMatrixf& x)
{
    DynamicMatrixf bx(std::max(b.numRows, x.numRows), 1);
    memcpy(bx.elements.data(), b.elements.data(), b.numRows * sizeof(float));

    DynamicMatrixf s(std::min(A.numRows, A.numCols), 1);

    auto rank = 0;
    LAPACKE_sgelss(LAPACK_COL_MAJOR, A.numRows, A.numCols, 1, const_cast<float*>(A.elements.data()), A.numRows,
                   const_cast<float*>(bx.elements.data()), bx.numRows, s.elements.data(), -1.0f, &rank);

    memcpy(x.elements.data(), bx.elements.data(), x.numRows * sizeof(float));
}

// Calculates the least squares solution of a linear system of equations, for double values.
template <>
void leastSquares(const DynamicMatrixd& A,
                  const DynamicMatrixd& b,
                  DynamicMatrixd& x)
{
    DynamicMatrixd bx(std::max(b.numRows, x.numRows), 1);
    memcpy(bx.elements.data(), b.elements.data(), b.numRows * sizeof(double));

    DynamicMatrixd s(std::min(A.numRows, A.numCols), 1);

    auto rank = 0;
    LAPACKE_dgelss(LAPACK_COL_MAJOR, A.numRows, A.numCols, 1, const_cast<double*>(A.elements.data()), A.numRows,
                   const_cast<double*>(bx.elements.data()), bx.numRows, s.elements.data(), -1.0, &rank);

    memcpy(x.elements.data(), bx.elements.data(), x.numRows * sizeof(double));
}

#endif

}
