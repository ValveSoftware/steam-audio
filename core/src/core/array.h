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

#include "error.h"
#include "memory_allocator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Array<T, N>
// --------------------------------------------------------------------------------------------------------------------

// Multidimensional array that can be accessed using standard multidimensional array notation, stores all its data
// contiguously (leading to very few memory allocations), and cleans up after itself. A 1D array can be treated as a T*,
// a 2D array as a T**, etc.
template <typename T, size_t N = 1>
class Array
{
public:
    size_t mSize;               // Size of the last dimension of this array. For an m x n array, this is n.
    unique_ptr<T[]> mElements;  // Contiguous buffer containing all elements of this array, in row-major order.
    Array<T*, N - 1> mPointers; // Array of rank N-1, containing pointers to "columns" of this array. For an
                                // m x n x k array, this is an m x n array of pointers, each pointing to k
                                // elements. Recursively, this will expand to a buffer of m * n elements,
                                // and an array of m pointers to n elements.
    size_t mSizes[N];

    // Default constructor creates an array of size 0.
    Array()
        : mSize(0)
        , mElements(nullptr)
        , mPointers()
    {}

    // Creates an array with the given dimensions.
    template <typename S, typename... Sizes>
    Array(S size, Sizes... sizes)
    {
        resize(size, sizes...);
    }

    ~Array()
    {}

    // Resizes this array to have the given dimensions.
    template <typename S, typename... Sizes>
    void resize(S size, Sizes... sizes)
    {
        auto sizeList = {sizes...};
        auto sizeArray = sizeList.begin();

        mSizes[0] = size;
        for (auto i = 1; i < N; ++i)
        {
            mSizes[i] = sizeArray[i - 1];
        }

        mSize = sizeArray[N - 2];

        auto numElements = size;
        for (auto i = 0; i < N - 1; ++i)
        {
            numElements *= sizeArray[i];
        }

        mElements = ipl::make_unique<T[]>(numElements);

        mPointers.resize(size, sizes...);

        auto stride = mSize;
        for (auto i = 0u; mSize > 0 && i < numElements / mSize; ++i)
        {
            mPointers.mElements[i] = &mElements[i * stride];
        }
    }

    // Access elements using array indexing syntax.
    const auto& operator[](const int i) const
    {
        return data()[i];
    }

    // Access elements using array indexing syntax.
    auto& operator[](const int i)
    {
        return data()[i];
    }

    // Returns the size of this array along the given dimension.
    size_t size(const int dim) const
    {
        return (dim == N - 1) ? mSize : mPointers.size(dim);
    }

    // Returns the total number of elements in this array.
    size_t totalSize() const
    {
        size_t result = 1;
        for (auto i = 0; i < N; ++i)
        {
            result *= size(i);
        }
        return result;
    }

    // Returns a pointer to the contiguous buffer containing all the elements.
    const T* flatData() const
    {
        return mElements.get();
    }

    // Returns a pointer to the contiguous buffer containing all the elements.
    T* flatData()
    {
        return mElements.get();
    }

    // Returns a pointer to the data. For a 1D array, this is a T*; for a 2D array, a T**, for a 3D array, T***, etc.
    auto data() const
    {
        return mPointers.data();
    }

    // Returns a pointer to the data. For a 1D array, this is a T*; for a 2D array, a T**, for a 3D array, T***, etc.
    auto data()
    {
        return mPointers.data();
    }

    // Clears all elements to zero.
    void zero()
    {
        memset(flatData(), 0, totalSize() * sizeof(T));
    }
};

// Base case specialization of Array<T, N> for the case of 1D arrays (N = 1).
template <typename T>
class Array<T, 1>
{
public:
    size_t mSize;
    unique_ptr<T[]> mElements;

    Array()
        : mSize(0)
        , mElements(nullptr)
    {}

    template <typename S, typename... Sizes>
    Array(S size, Sizes... sizes)
    {
        resize(size, sizes...);
    }

    ~Array()
    {}

    template <typename S, typename... Sizes>
    void resize(S size, Sizes... sizes)
    {
        mSize = size;

        mElements = ipl::make_unique<T[]>(mSize);
    }

    const T& operator[](const int i) const
    {
        return data()[i];
    }

    T& operator[](const int i)
    {
        return data()[i];
    }

    size_t size(const int dim) const
    {
        return mSize;
    }

    size_t totalSize() const
    {
        return size(0);
    }

    const T* flatData() const
    {
        return mElements.get();
    }

    T* flatData()
    {
        return mElements.get();
    }

    auto data() const
    {
        return mElements.get();
    }

    auto data()
    {
        return mElements.get();
    }

    void zero()
    {
        memset(flatData(), 0, totalSize() * sizeof(T));
    }
};

}
