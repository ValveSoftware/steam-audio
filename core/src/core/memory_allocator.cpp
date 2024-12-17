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

#include "memory_allocator.h"
#include "error.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Memory
// --------------------------------------------------------------------------------------------------------------------

void Memory::init(AllocateCallback allocateCallback,
                  FreeCallback freeCallback)
{
    mAllocateCallback = allocateCallback;
    mFreeCallback = freeCallback;
}

void* Memory::allocate(size_t size,
                       size_t alignment)
{
    void* pointer = nullptr;

    if (mAllocateCallback)
    {
        pointer = mAllocateCallback(size, alignment);
    }
    else
    {
#if defined(IPL_OS_WINDOWS)
        pointer = _aligned_malloc(size, alignment);
#else
        posix_memalign(&pointer, alignment, size);
#endif
    }

    if (!pointer && size > 0)
        throw Exception(Status::OutOfMemory);

    return pointer;
}

void Memory::free(void* memblock)
{
    if (mFreeCallback)
    {
        mFreeCallback(memblock);
    }
    else
    {
#if defined(IPL_OS_WINDOWS)
        _aligned_free(memblock);
#else
        ::free(memblock);
#endif
    }
}

}
