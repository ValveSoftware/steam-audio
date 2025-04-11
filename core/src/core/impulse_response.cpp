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

#include "impulse_response.h"

#include "array_math.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ImpulseResponse
// --------------------------------------------------------------------------------------------------------------------

ImpulseResponse::ImpulseResponse(float duration,
                                 int order,
                                 int samplingRate)
{
    mData.resize(SphericalHarmonics::numCoeffsForOrder(order), static_cast<int>(ceilf(duration * samplingRate)));
    reset();
}

void ImpulseResponse::reset()
{
    mData.zero();
}

void ImpulseResponse::copy(const ImpulseResponse& src, ImpulseResponse& dst)
{
    auto numChannelsToCopy = std::min(src.numChannels(), dst.numChannels());
    auto numSamplesToCopy = std::min(src.numSamples(), dst.numSamples());

    for (auto i = 0; i < numChannelsToCopy; ++i)
    {
        memcpy(dst.mData[i], src.mData[i], numSamplesToCopy * sizeof(float));
    }
}

void ImpulseResponse::swap(ImpulseResponse& a, ImpulseResponse& b)
{
    a.mData.swap(b.mData);
}

void ImpulseResponse::add(const ImpulseResponse& in1, const ImpulseResponse& in2, ImpulseResponse& out)
{
    auto numChannels = std::min({in1.numChannels(), in2.numChannels(), out.numChannels()});
    auto numSamples = std::min({in1.numSamples(), in2.numSamples(), out.numSamples()});

    for (auto i = 0; i < numChannels; ++i)
    {
        ArrayMath::add(numSamples, in1[i], in2[i], out[i]);
    }
}

void ImpulseResponse::scale(const ImpulseResponse& in, float scalar, ImpulseResponse& out)
{
    auto numChannels = std::min(in.numChannels(), out.numChannels());
    auto numSamples = std::min(in.numSamples(), out.numSamples());

    for (auto i = 0; i < numChannels; ++i)
    {
        ArrayMath::scale(numSamples, in[i], scalar, out[i]);
    }
}

void ImpulseResponse::scaleAccumulate(const ImpulseResponse& in, float scalar, ImpulseResponse& out)
{
    auto numChannels = std::min(in.numChannels(), out.numChannels());
    auto numSamples = std::min(in.numSamples(), out.numSamples());

    for (auto i = 0; i < numChannels; ++i)
    {
        ArrayMath::scaleAccumulate(numSamples, in[i], scalar, out[i]);
    }
}

}
