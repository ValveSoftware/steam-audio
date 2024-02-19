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

#include "array.h"
#include "serialized_object.h"

#include "energy_field.fbs.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EnergyField
// --------------------------------------------------------------------------------------------------------------------

class EnergyField
{
public:
    static const float kBinDuration;

    EnergyField(float duration,
                int order);

    EnergyField(const Serialized::EnergyField* serializedObject);

    virtual ~EnergyField()
    {}

    int numChannels() const
    {
        return static_cast<int>(mData.size(0));
    }

    int numBins() const
    {
        return static_cast<int>(mData.size(2));
    }

    float* flatData()
    {
        return mData.flatData();
    }

    const float* flatData() const
    {
        return mData.flatData();
    }

    float* const* const* data()
    {
        return mData.data();
    }

    const float* const* const* data() const
    {
        return mData.data();
    }

    float* const* operator[](int i)
    {
        return mData[i];
    }

    const float* const* operator[](int i) const
    {
        return mData[i];
    }

    virtual void reset();

    uint64_t serializedSize() const;

    flatbuffers::Offset<Serialized::EnergyField> serialize(SerializedObject& serializedObject) const;

    void copyFrom(const EnergyField& other);

    static void add(const EnergyField& in1,
                    const EnergyField& in2,
                    EnergyField& out);

    static void scale(const EnergyField& in,
                      float scalar,
                      EnergyField& out);

    static void scaleAccumulate(const EnergyField& in,
                                float scalar,
                                EnergyField& out);

protected:
    Array<float, 3> mData;
};

}
