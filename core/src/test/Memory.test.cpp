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

#include <memory_allocator.h>
#include <context.h>
#include <types.h>
#include <containers.h>
#include <array.h>

#if defined(IPL_OS_WINDOWS) && !defined(_DEBUG)

size_t gBytesAllocated = 0;

void* IPL_CALLBACK allocateMemory(const size_t size, const size_t alignment)
{
    gBytesAllocated += size;
    return malloc(size);
}

void IPL_CALLBACK freeMemory(void* memoryBlock)
{
    free(memoryBlock);
}

int measureHeap()
{
    auto heap = GetProcessHeap();
    HeapLock(heap);

    PROCESS_HEAP_ENTRY heapEntry;
    heapEntry.lpData = nullptr;

    auto size = 0;
    while (HeapWalk(heap, &heapEntry))
        if (heapEntry.wFlags & (PROCESS_HEAP_ENTRY_BUSY | PROCESS_HEAP_ENTRY_MOVEABLE))
            size += heapEntry.cbData;

    HeapUnlock(heap);

    return size;
}

TEST_CASE("Memory::allocate allocations are routed correctly.", "[memory]")
{
    gBytesAllocated = 0;
    ipl::gMemory().init(allocateMemory, freeMemory);

    auto baseline = measureHeap();
    auto memoryBlock = ipl::gMemory().allocate(1024, ipl::Memory::kDefaultAlignment);
    auto bytesAllocated = measureHeap() - baseline;

    ipl::gMemory().free(memoryBlock);

    ipl::gMemory().init(nullptr, nullptr);
    REQUIRE(bytesAllocated == gBytesAllocated);
}

TEST_CASE("STL container allocations are routed correctly.", "[memory]")
{
    gBytesAllocated = 0;
    ipl::gMemory().init(allocateMemory, freeMemory);

    auto bytesAllocated = 0;

    {
        auto baseline = measureHeap();
        ipl::vector<float> container(32);
        bytesAllocated = measureHeap() - baseline;
    }

    ipl::gMemory().init(nullptr, nullptr);
    REQUIRE(bytesAllocated == gBytesAllocated);
}

struct BigObject
{
    ipl::vector<float> data;

    BigObject(const size_t numElements) :
        data(numElements)
    {}
};

TEST_CASE("make_unique allocations are routed correctly.", "[memory]")
{
    gBytesAllocated = 0;
    ipl::gMemory().init(allocateMemory, freeMemory);

    auto bytesAllocated = 0;

    {
        auto baseline = measureHeap();
        auto bigObject = ipl::make_unique<BigObject>(32);
        bytesAllocated = measureHeap() - baseline;
    }

    ipl::gMemory().init(nullptr, nullptr);
    REQUIRE(bytesAllocated == gBytesAllocated);
}

TEST_CASE("make_shared allocations are routed correctly.", "[memory]")
{
    gBytesAllocated = 0;
    ipl::gMemory().init(allocateMemory, freeMemory);

    auto bytesAllocated = 0;

    {
        auto baseline = measureHeap();
        auto bigObject = ipl::make_shared<BigObject>(32);
        bytesAllocated = measureHeap() - baseline;
    }

    ipl::gMemory().init(nullptr, nullptr);
    REQUIRE(bytesAllocated == gBytesAllocated);
}

TEST_CASE("Array<T> allocations are routed correctly.", "[memory]")
{
    gBytesAllocated = 0;
    ipl::gMemory().init(allocateMemory, freeMemory);

    auto bytesAllocated = 0;

    {
        auto baseline = measureHeap();
        ipl::Array<float> container(32);
        bytesAllocated = measureHeap() - baseline;
    }

    ipl::gMemory().init(nullptr, nullptr);
    REQUIRE(bytesAllocated == gBytesAllocated);
}

TEST_CASE("Array<T, 2> allocations are routed correctly.", "[memory]")
{
    gBytesAllocated = 0;
    ipl::gMemory().init(allocateMemory, freeMemory);

    auto bytesAllocated = 0;

    {
        auto baseline = measureHeap();
        ipl::Array<float, 2> container(32, 5);
        bytesAllocated = measureHeap() - baseline;
    }

    ipl::gMemory().init(nullptr, nullptr);
    REQUIRE(bytesAllocated == gBytesAllocated);
}

#endif
