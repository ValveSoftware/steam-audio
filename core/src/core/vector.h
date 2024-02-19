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

#include <algorithm>

#include "error.h"
#include "math_functions.h"
#include "memory_allocator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Vector<T, D>
// --------------------------------------------------------------------------------------------------------------------

// Base class template for stack-allocated geometric vectors of arbitrary size.
template <typename T, const int D>
class Vector
{
public:
    static const Vector<T, D> kZero; // The zero vector.

    T elements[D]; // Elements of the vector.

    // The default constructor initializes the vector to zero.
    Vector()
    {
        memset(elements, 0, D * sizeof(T));
    }

    // Initializes the vector using an initializer list. The number of elements in the initializer list must
    // match the value of the template parameter D.
    Vector(std::initializer_list<T> l)
    {
        assert(l.size() == D);
        std::uninitialized_copy(l.begin(), l.end(), elements);
    }

    // Constructs the vector using an array of values. The number of elements in the array must match the value
    // of the template parameter D.
    explicit Vector(const T* values)
    {
        memcpy(elements, values, D * sizeof(T));
    }

    const T& operator[](int i) const
    {
        return elements[i];
    }

    T& operator[](int i)
    {
        return elements[i];
    }

    //* Adds another vector to this vector.
    Vector<T, D>& operator+=(const Vector<T, D>& rhs)
    {
        for (auto i = 0; i < D; ++i)
        {
            elements[i] += rhs[i];
        }

        return *this;
    }

    // Subtracts another vector from this vector.
    Vector<T, D>& operator-=(const Vector<T, D>& rhs)
    {
        for (auto i = 0; i < D; ++i)
        {
            elements[i] -= rhs[i];
        }

        return *this;
    }

    // Element-wise multiplies another vector with this vector.
    Vector<T, D>& operator*=(const Vector<T, D>& rhs)
    {
        for (auto i = 0; i < D; ++i)
        {
            elements[i] *= rhs[i];
        }

        return *this;
    }

    // Uniformly scales the elements of this vector.
    Vector<T, D>& operator*=(const T& s)
    {
        for (auto i = 0; i < D; ++i)
        {
            elements[i] *= s;
        }

        return *this;
    }

    // Element-wise divides this vector by another vector.
    Vector<T, D>& operator/=(const Vector<T, D>& rhs)
    {
        assert(!(rhs == kZero));

        for (auto i = 0; i < D; ++i)
        {
            elements[i] *= static_cast<T>(1) / rhs[i];
        }

        return *this;
    }

    // Uniformly scales the elements of this vector.
    Vector<T, D>& operator/=(const T& s)
    {
        assert(s != 0);

        for (auto i = 0; i < D; ++i)
        {
            elements[i] *= static_cast<T>(1) / s;
        }

        return *this;
    }

    // Returns the element of the vector with the minimum value.
    T minComponent() const
    {
        auto minElementItr = std::min_element(std::begin(elements), std::end(elements));
        return elements[std::distance(std::begin(elements), minElementItr)];
    }

    // Returns the element of the vector with the maximum value.
    T maxComponent() const
    {
        auto maxElementItr = std::max_element(std::begin(elements), std::end(elements));
        return elements[std::distance(std::begin(elements), maxElementItr)];
    }

    // Returns the element of the vector with the minimum absolute value.
    T minAbsComponent() const
    {
        auto compareFunc = [](const T& a, const T& b)
        {
            return std::abs(a) < std::abs(b);
        };

        auto minElementItr = std::min_element(std::begin(elements), std::end(elements), compareFunc);
        return elements[std::distance(std::begin(elements), minElementItr)];
    }

    // Returns the element of the vector with the maximum absolute value.
    T maxAbsComponent() const
    {
        auto compareFunc = [](const T& a, const T& b)
        {
            return std::abs(a) < std::abs(b);
        };

        auto maxElementItr = std::max_element(std::begin(elements), std::end(elements), compareFunc);
        return elements[std::distance(std::begin(elements), maxElementItr)];
    }

    // Returns the index of the element of the vector with the minimum value.
    int indexOfMinComponent() const
    {
        auto minElementItr = std::min_element(std::begin(elements), std::end(elements));
        return static_cast<int>(std::distance(std::begin(elements), minElementItr));
    }

    // Returns the index of the element of the vector with the maximum value.
    int indexOfMaxComponent() const
    {
        auto maxElementItr = std::max_element(std::begin(elements), std::end(elements));
        return static_cast<int>(std::distance(std::begin(elements), maxElementItr));
    }

    // Returns the index of the element of the vector with the minimum absolute value.
    int indexOfMinAbsComponent() const
    {
        auto compareFunc = [](const T& a, const T& b)
        {
            return std::abs(a) < std::abs(b);
        };

        auto minElementItr = std::min_element(std::begin(elements), std::end(elements), compareFunc);
        return static_cast<int>(std::distance(std::begin(elements), minElementItr));
    }

    // Returns the index of the element of the vector with the maximum absolute value.
    int indexOfMaxAbsComponent() const
    {
        auto compareFunc = [](const T& a, const T& b)
        {
            return std::abs(a) < std::abs(b);
        };

        auto maxElementItr = std::max_element(std::begin(elements), std::end(elements), compareFunc);
        return static_cast<int>(std::distance(std::begin(elements), maxElementItr));
    }

    // Calculates the squared length of this vector.
    T lengthSquared() const
    {
        return dot(*this, *this);
    }

    // Calculates the length of this vector.
    T length() const
    {
        return std::sqrt(lengthSquared());
    }

    // Calculates the scalar (dot) product between two vectors. Both vectors must have the same type and length.
    static T dot(const Vector<T, D>& lhs,
                 const Vector<T, D>& rhs)
    {
        T out = 0;

        for (auto i = 0; i < D; ++i)
        {
            out += lhs[i] * rhs[i];
        }

        return out;
    }

    // Returns a vector whose elements are the element-wise minimum of the elements of two given vectors.
    static Vector<T, D> min(const Vector<T, D>& lhs,
                            const Vector<T, D>& rhs)
    {
        Vector<T, D> out;

        for (auto i = 0; i < D; ++i)
        {
            out[i] = std::min(lhs[i], rhs[i]);
        }

        return out;
    }

    // Returns a vector whose elements are the element-wise maximum of the elements of two given vectors.
    static Vector<T, D> max(const Vector<T, D>& lhs,
                            const Vector<T, D>& rhs)
    {
        Vector<T, D> out;

        for (auto i = 0; i < D; ++i)
        {
            out[i] = std::max(lhs[i], rhs[i]);
        }

        return out;
    }

    // Returns a vector whose elements are the reciprocals of the elements in the given vector.
    static Vector<T, D> reciprocal(const Vector<T, D>& v)
    {
        Vector<T, D> out;

        for (auto i = 0; i < D; ++i)
        {
            out[i] = static_cast<T>(1) / v[i];
        }

        return out;
    }

    // Returns a vector whose elements are the square roots of the elements in the given vector.
    static Vector<T, D> sqrt(const Vector<T, D>& v)
    {
        Vector<T, D> out;

        for (auto i = 0; i < D; ++i)
        {
            out[i] = std::sqrt(v[i]);
        }

        return out;
    }
};

// Returns the negative of a vector.
template <typename T, const int D>
Vector<T, D> operator-(const Vector<T, D>& v)
{
    Vector<T, D> out;

    for (auto i = 0; i < D; ++i)
    {
        out[i] = -v[i];
    }

    return out;
}

// Adds two vectors.
template <typename T, const int D>
Vector<T, D> operator+(const Vector<T, D>& lhs,
                       const Vector<T, D>& rhs)
{
    Vector<T, D> out(lhs);
    out += rhs;
    return out;
}

// Subtracts two vectors.
template <typename T, const int D>
Vector<T, D> operator-(const Vector<T, D>& lhs,
                       const Vector<T, D>& rhs)
{
    Vector<T, D> out(lhs);
    out -= rhs;
    return out;
}

// Element-wise multiples two vectors.
template <typename T, const int D>
Vector<T, D> operator*(const Vector<T, D>& lhs,
                       const Vector<T, D>& rhs)
{
    Vector<T, D> out(lhs);
    out *= rhs;
    return out;
}

// Uniformly scales a vector.
template <typename T, const int D>
Vector<T, D> operator*(const Vector<T, D>& v,
                       const T& s)
{
    Vector<T, D> out(v);
    out *= s;
    return out;
}

// Uniformly scales a vector.
template <typename T, const int D>
Vector<T, D> operator*(const T& s,
                       const Vector<T, D>& v)
{
    Vector<T, D> out(v);
    out *= s;
    return out;
}

// Element-wise divides two vectors.
template <typename T, const int D>
Vector<T, D> operator/(const Vector<T, D>& lhs,
                       const Vector<T, D>& rhs)
{
    assert(!(rhs == Vector<T, D>::kZero));
    Vector<T, D> out(lhs);
    lhs /= rhs;
    return out;
}

// Uniformly scales a vector.
template <typename T, const int D>
Vector<T, D> operator/(const Vector<T, D>& v,
                       const T& s)
{
    assert(s != 0);
    Vector<T, D> out(v);
    out /= s;
    return out;
}

// Checks whether two vectors are element-wise equal.
template <typename T, const int D>
bool operator==(const Vector<T, D>& lhs,
                const Vector<T, D>& rhs)
{
    for (auto i = 0; i < D; ++i)
    {
        if (lhs[i] != rhs[i])
            return false;
    }

    return true;
}

// Checks whether two vectors are not element-wise equal.
template <typename T, const int D>
bool operator!=(const Vector<T, D>& lhs,
                const Vector<T, D>& rhs)
{
    return !(lhs == rhs);
}

// Checks whether one vector is element-wise greater than another.
template <typename T, const int D>
bool operator>(const Vector<T, D>& lhs,
               const Vector<T, D>& rhs)
{
    for (auto i = 0; i < D; ++i)
    {
        if (lhs[i] <= rhs[i])
            return false;
    }

    return true;
}

// Checks whether one vector is element-wise greater than or equal to another.
template <typename T, const int D>
bool operator>=(const Vector<T, D>& lhs,
                const Vector<T, D>& rhs)
{
    for (auto i = 0; i < D; ++i)
    {
        if (lhs[i] < rhs[i])
            return false;
    }

    return true;
}

// Checks whether one vector is element-wise lesser than another.
template <typename T, const int D>
bool operator<(const Vector<T, D>& lhs,
               const Vector<T, D>& rhs)
{
    for (auto i = 0; i < D; ++i)
    {
        if (lhs[i] >= rhs[i])
            return false;
    }

    return true;
}

// Checks whether one vector is element-wise lesser than or equal to another.
template <typename T, const int D>
bool operator<=(const Vector<T, D>& lhs,
                const Vector<T, D>& rhs)
{
    for (auto i = 0; i < D; ++i)
    {
        if (lhs[i] > rhs[i])
            return false;
    }

    return true;
}

template <typename T, const int D>
const Vector<T, D> Vector<T, D>::kZero;


// --------------------------------------------------------------------------------------------------------------------
// Vector2<T>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents two-dimensional vectors.
template <typename T>
class Vector2 : public Vector<T, 2>
{
public:
    static const Vector2<T> kXAxis; // Unit vector pointing along the x axis.
    static const Vector2<T> kYAxis; // Unit vector pointing along the y axis.

    // Initializes the vector to zero.
    Vector2()
        : Vector<T, 2>()
    {}

    // Copy constructs the vector from another.
    Vector2(const Vector<T, 2>& values)
        : Vector<T, 2>(values)
    {}

    // Initializes the vector using given x and y coordinates.
    Vector2(const T& x,
            const T& y)
    {
        this->elements[0] = x;
        this->elements[1] = y;
    }

    const T& x() const
    {
        return this->elements[0];
    }

    T& x()
    {
        return this->elements[0];
    }

    const T& y() const
    {
        return this->elements[1];
    }

    T& y()
    {
        return this->elements[1];
    }
};

template<typename T>
const Vector2<T> Vector2<T>::kXAxis(1, 0);

template<typename T>
const Vector2<T> Vector2<T>::kYAxis(0, 1);

typedef Vector2<float> Vector2f;
typedef Vector2<double> Vector2d;


// --------------------------------------------------------------------------------------------------------------------
// Vector3<T>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents three-dimensional vectors.
template <typename T>
class Vector3 : public Vector<T, 3>
{
public:
    static const Vector3<T> kXAxis; // Unit vector pointing along the x axis.
    static const Vector3<T> kYAxis; // Unit vector pointing along the y axis.
    static const Vector3<T> kZAxis; // Unit vector pointing along the z axis.
    static constexpr T kNearlyZero = static_cast<T>(1e-5); // Epsilon value that is close to zero.

    // Initializes the vector to zero.
    Vector3()
        : Vector<T, 3>()
    {}

    // Copy constructs the vector from another.
    Vector3(const Vector<T, 3>& values)
        : Vector<T, 3>(values)
    {}

    // Initializes the vector using given x, y, and z coordinates.
    Vector3(const T& x,
            const T& y,
            const T& z)
    {
        this->elements[0] = x;
        this->elements[1] = y;
        this->elements[2] = z;
    }

    const T& x() const
    {
        return this->elements[0];
    }

    T& x()
    {
        return this->elements[0];
    }

    const T& y() const
    {
        return this->elements[1];
    }

    T& y()
    {
        return this->elements[1];
    }

    const T& z() const
    {
        return this->elements[2];
    }

    T& z()
    {
        return this->elements[2];
    }

    // Returns a unit vector pointing in the direction of a given vector.
    static Vector3<T> unitVector(const Vector3<T>& v)
    {
        auto length = v.length();
        if (length <= kNearlyZero)
        {
            return Vector3<T>::kZero;
        }
        else
        {
            return v / length;
        }
    }

    // Calculates the vector (cross) product of two vectors.
    static Vector3<T> cross(const Vector3<T>& lhs,
                            const Vector3<T>& rhs)
    {
        return Vector3<T>(lhs.y() * rhs.z() - lhs.z() * rhs.y(),
                          lhs.z() * rhs.x() - lhs.x() * rhs.z(),
                          lhs.x() * rhs.y() - lhs.y() * rhs.x());
    }

    // Calculates the angle between two vectors, in the range [0, 2*pi).
    static T angleBetween(const Vector3<T>& from,
                          const Vector3<T>& to)
    {
        auto parallel = Vector3::dot(to, from) * from;
        auto perpendicular = to - parallel;

        auto x = parallel.length();
        auto y = perpendicular.length();

        auto angle = std::atan2(y, x);
        if (angle < 0)
        {
            angle += 2 * static_cast<T>(Math::kPiD);
        }

        return angle;
    }

    static Vector3<T> reflect(const Vector3<T>& incident,
                              const Vector3<T>& normal)
    {
        return unitVector((incident - (normal * 2.0f * Vector3<T>::dot(incident, normal))));
    }
};

template <typename T>
const Vector3<T> Vector3<T>::kXAxis(1, 0, 0);

template <typename T>
const Vector3<T> Vector3<T>::kYAxis(0, 1, 0);

template <typename T>
const Vector3<T> Vector3<T>::kZAxis(0, 0, 1);

typedef Vector3<float> Vector3f;
typedef Vector3<double> Vector3d;


// --------------------------------------------------------------------------------------------------------------------
// Vector4<T>
// --------------------------------------------------------------------------------------------------------------------

// Template class that represents four-dimensional (homogeneous) vectors.
template <typename T>
class Vector4 : public Vector<T, 4>
{
public:
    static const Vector4<T> kXAxis; // Unit vector pointing along the x axis.
    static const Vector4<T> kYAxis; // Unit vector pointing along the y axis.
    static const Vector4<T> kZAxis; // Unit vector pointing along the z axis.
    static const Vector4<T> kWAxis; // Unit vector pointing along the w axis.

    // Initializes the vector to zero.
    Vector4()
        : Vector<T, 4>()
    {}

    // Initializes a homogeneous vector from a 3D vector. The 3D vector is assumed to be a point, so the w
    // coordinate of the homogeneous vector is set to 1.
    Vector4(const Vector3<T>& values)
    {
        this->elements[0] = values.x();
        this->elements[1] = values.y();
        this->elements[2] = values.z();
        this->elements[3] = 1;
    }

    // Copy constructs the vector from another.
    Vector4(const Vector<T, 4>& values)
        : Vector<T, 4>(values)
    {}

    // Initializes the vector using given x, y, z, and w coordinates.
    Vector4(const T& x,
            const T& y,
            const T& z,
            const T& w)
    {
        this->elements[0] = x;
        this->elements[1] = y;
        this->elements[2] = z;
        this->elements[3] = w;
    }

    const T& x() const
    {
        return this->elements[0];
    }

    T& x()
    {
        return this->elements[0];
    }

    const T& y() const
    {
        return this->elements[1];
    }

    T& y()
    {
        return this->elements[1];
    }

    const T& z() const
    {
        return this->elements[2];
    }

    T& z()
    {
        return this->elements[2];
    }

    const T& w() const
    {
        return this->elements[3];
    }

    T& w()
    {
        return this->elements[3];
    }
};

template<typename T>
const Vector4<T> Vector4<T>::kXAxis(1, 0, 0, 0);

template<typename T>
const Vector4<T> Vector4<T>::kYAxis(0, 1, 0, 0);

template<typename T>
const Vector4<T> Vector4<T>::kZAxis(0, 0, 1, 0);

template<typename T>
const Vector4<T> Vector4<T>::kWAxis(0, 0, 0, 1);

typedef Vector4<float> Vector4f;
typedef Vector4<double> Vector4d;

}
