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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ImpulseResponse
// --------------------------------------------------------------------------------------------------------------------

class ImpulseResponse
{
public:
    ImpulseResponse(float duration,
                    int order,
                    int samplingRate);

    virtual ~ImpulseResponse()
    {}

    int numChannels() const
    {
        return static_cast<int>(mData.size(0));
    }

    int numSamples() const
    {
        return static_cast<int>(mData.size(1));
    }

    float* data()
    {
        return mData.flatData();
    }

    const float* data() const
    {
        return mData.flatData();
    }

    float* operator[](int i)
    {
        return mData[i];
    }

    const float* operator[](int i) const
    {
        return mData[i];
    }

    virtual void reset();

protected:
    Array<float, 2> mData;
};

}
