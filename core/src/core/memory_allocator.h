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

#include <stdlib.h>
#include <string.h>

#include <new>

#include "types.h"

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define alignas(x) __declspec(align(x))
#endif

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Memory
// --------------------------------------------------------------------------------------------------------------------

typedef void* (IPL_CALLBACK *AllocateCallback)(size_t size, size_t alignment);
typedef void (IPL_CALLBACK *FreeCallback)(void* memblock);

class Memory
{
public:
    static const int kDefaultAlignment = 64;

    void init(AllocateCallback allocateCallback,
              FreeCallback freeCallback);

    void* allocate(size_t size,
                   size_t alignment);

    void free(void* memblock);

private:
    AllocateCallback mAllocateCallback;
    FreeCallback mFreeCallback;
};

extern Memory& gMemory();


// --------------------------------------------------------------------------------------------------------------------
// allocator<T>
// --------------------------------------------------------------------------------------------------------------------

// An STL-compliant allocator that routes all allocations through the global memory system.
template <typename T>
class allocator
{
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = value_type * ;
    using const_pointer = typename std::pointer_traits<pointer>::template rebind<value_type const>;
    using void_pointer = typename std::pointer_traits<pointer>::template rebind<void>;
    using const_void_pointer = typename std::pointer_traits<pointer>::template rebind<const void>;
    using difference_type = typename std::pointer_traits<pointer>::difference_type;
    using size_type = std::make_unsigned_t<difference_type>;

    template <class U>
    struct rebind
    {
        typedef allocator<U> other;
    };

    allocator() noexcept
    {}

    template <class U>
    allocator(const allocator<U>&) noexcept
    {}

    T* allocate(size_t n)
    {
        return reinterpret_cast<T*>(gMemory().allocate(n * sizeof(T), Memory::kDefaultAlignment));
    }

    void deallocate(T* p,
                    size_t num)
    {
        gMemory().free(p);
    }

    value_type* allocate(std::size_t n,
                         const_void_pointer)
    {
        return allocate(n);
    }

    template <class U, class ...Args>
    void construct(U* p,
                   Args&& ...args)
    {
        new ((void*) p) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U* p) noexcept
    {
        p->~U();
    }

    std::size_t max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max();
    }

    allocator select_on_container_copy_construction() const
    {
        return *this;
    }

    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::is_empty<allocator>;
};

template <typename T1, typename T2>
bool operator==(const allocator<T1>&,
                const allocator<T2>&) noexcept
{
    return true;
}

template <typename T1, typename T2>
bool operator!=(const allocator<T1>&,
                const allocator<T2>&) noexcept
{
    return false;
}


// --------------------------------------------------------------------------------------------------------------------
// unique_ptr<T>
// --------------------------------------------------------------------------------------------------------------------

struct deleter
{
    template <typename T>
    void operator()(T* p)
    {
        p->~T();
        gMemory().free(p);
    }
};

template <typename T>
using unique_ptr = std::unique_ptr<T, deleter>;

template <typename T, typename... Args>
typename std::enable_if<!std::is_array<T>::value, ipl::unique_ptr<T>>::type make_unique(Args&&... args)
{
    auto p = reinterpret_cast<T*>(gMemory().allocate(sizeof(T), Memory::kDefaultAlignment));
    new (p) T(std::forward<Args>(args)...);
    return ipl::unique_ptr<T>(p, deleter());
}

template <typename T, typename... Args>
typename std::enable_if<std::is_array<T>::value && std::extent<T>::value == 0, ipl::unique_ptr<T>>::type make_unique(size_t size)
{
    typedef typename std::remove_extent<T>::type E;
    auto p = reinterpret_cast<E*>(gMemory().allocate(size * sizeof(E), Memory::kDefaultAlignment));

    for (auto i = 0u; i < size; ++i)
    {
        new (&p[i]) E();
    }

    return ipl::unique_ptr<T>(p, deleter());
}


// --------------------------------------------------------------------------------------------------------------------
// shared_ptr<T>
// --------------------------------------------------------------------------------------------------------------------

template <typename T>
using shared_ptr = std::shared_ptr<T>;

template <typename T, typename... Args>
std::shared_ptr<T> make_shared(Args&&... args)
{
    return std::allocate_shared<T>(allocator<T>(), std::forward<Args>(args)...);
}

}

