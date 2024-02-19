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

#include "energy_field.h"

#include "array_math.h"
#include "bands.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EnergyField
// --------------------------------------------------------------------------------------------------------------------

const float EnergyField::kBinDuration = 1e-2f;

EnergyField::EnergyField(float duration,
                         int order)
{
    mData.resize(SphericalHarmonics::numCoeffsForOrder(order), Bands::kNumBands,
                 static_cast<int>(ceilf(duration / kBinDuration)));

    reset();
}

EnergyField::EnergyField(const Serialized::EnergyField* serializedObject)
{
    assert(serializedObject);
    assert(serializedObject->num_channels() > 0);
    assert(serializedObject->num_bins() > 0);
    assert(serializedObject->data());

    auto numChannels = serializedObject->num_channels();
    auto numBins = serializedObject->num_bins();

    mData.resize(numChannels, Bands::kNumBands, numBins);

    memcpy(mData.flatData(), serializedObject->data()->data(), mData.totalSize() * sizeof(float));
}

void EnergyField::reset()
{
    mData.zero();
}

uint64_t EnergyField::serializedSize() const
{
    return (2 * sizeof(int32_t) +
            mData.totalSize() * sizeof(float));
}

flatbuffers::Offset<Serialized::EnergyField> EnergyField::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    auto dataOffset = fbb.CreateVector(mData.flatData(), mData.totalSize());

    return Serialized::CreateEnergyField(fbb, numChannels(), numBins(), dataOffset);
}

void EnergyField::copyFrom(const EnergyField& other)
{
    auto numChannelsToCopy = std::min(numChannels(), other.numChannels());
    auto numBinsToCopy = std::min(numBins(), other.numBins());

    for (auto i = 0; i < numChannelsToCopy; ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            memcpy(mData[i][j], other.mData[i][j], numBinsToCopy * sizeof(float));
        }
    }
}

void EnergyField::add(const EnergyField& in1,
                      const EnergyField& in2,
                      EnergyField& out)
{
    auto numChannels = std::min({in1.numChannels(), in2.numChannels(), out.numChannels()});
    auto numBins = std::min({in1.numBins(), in2.numBins(), out.numBins()});

    for (auto i = 0; i < numChannels; ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            ArrayMath::add(numBins, in1[i][j], in2[i][j], out[i][j]);
        }
    }
}

void EnergyField::scale(const EnergyField& in,
                        float scalar,
                        EnergyField& out)
{
    auto numChannels = std::min({in.numChannels(), out.numChannels()});
    auto numBins = std::min({in.numBins(), out.numBins()});

    for (auto i = 0; i < numChannels; ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            ArrayMath::scale(numBins, in[i][j], scalar, out[i][j]);
        }
    }
}

void EnergyField::scaleAccumulate(const EnergyField& in,
                                  float scalar,
                                  EnergyField& out)
{
    auto numChannels = std::min({in.numChannels(), out.numChannels()});
    auto numBins = std::min({in.numBins(), out.numBins()});

    for (auto i = 0; i < numChannels; ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            ArrayMath::scaleAccumulate(numBins, in[i][j], scalar, out[i][j]);
        }
    }
}

}
