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

#include <flatbuffers/flatbuffers.h>

#include "memory_allocator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SerializedObject
// --------------------------------------------------------------------------------------------------------------------

class FlatBuffersAllocator;

class SerializedObject
{
public:
    SerializedObject();
    SerializedObject(size_t size, const byte_t* data);

    size_t size() { return mSize; }
    const byte_t* data() const { return mData; }
    flatbuffers::FlatBufferBuilder& fbb() { return *mFBB; }
    const flatbuffers::FlatBufferBuilder& fbb() const { return *mFBB; }

    void commit();

private:
    static const int kInitialSize;

    unique_ptr<flatbuffers::FlatBufferBuilder> mFBB;
    size_t mSize;
    const byte_t* mData;

    static FlatBuffersAllocator sAllocator;
};

}

