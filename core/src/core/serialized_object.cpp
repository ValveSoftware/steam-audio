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

#include "serialized_object.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// FlatBuffersAllocator
// --------------------------------------------------------------------------------------------------------------------

class FlatBuffersAllocator : public flatbuffers::Allocator
{
public:
    virtual uint8_t* allocate(size_t size) override
    {
        return reinterpret_cast<uint8_t*>(gMemory().allocate(size, Memory::kDefaultAlignment));
    }

    virtual void deallocate(uint8_t* p,
                            size_t size) override
    {
        gMemory().free(p);
    }
};


// --------------------------------------------------------------------------------------------------------------------
// SerializedObject
// --------------------------------------------------------------------------------------------------------------------

const int SerializedObject::kInitialSize = 1024;

FlatBuffersAllocator SerializedObject::sAllocator;

SerializedObject::SerializedObject()
    : mFBB(ipl::make_unique<flatbuffers::FlatBufferBuilder>(kInitialSize, &sAllocator))
    , mSize(0)
    , mData(nullptr)
{}

SerializedObject::SerializedObject(size_t size, const byte_t* data)
    : mSize(size)
    , mData(data)
{}

void SerializedObject::commit()
{
    mSize = mFBB->GetSize();
    mData = mFBB->GetBufferPointer();
}

}

