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

#include "baked_reflection_data.h"

#include "thread_pool.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// BakedReflectionsData
// ---------------------------------------------------------------------------------------------------------------------

BakedReflectionsData::BakedReflectionsData(const BakedDataIdentifier& identifier,
                                           int numProbes,
                                           bool hasConvolution,
                                           bool hasParametric)
    : mIdentifier(identifier)
    , mHasConvolution(hasConvolution)
    , mHasParametric(hasParametric)
    , mNeedsUpdate(numProbes)
{
    if (hasConvolution)
    {
        mEnergyFields.resize(numProbes);
    }

    if (hasParametric)
    {
        mReverbs.resize(numProbes);
    }

    for (auto i = 0; i < numProbes; ++i)
    {
        mNeedsUpdate[i] = true;
    }
}

BakedReflectionsData::BakedReflectionsData(const BakedDataIdentifier& identifier,
                                           int numProbes,
                                           const Serialized::BakedReflectionsData* serializedObject)
    : mIdentifier(identifier)
    , mNeedsUpdate(numProbes)
{
    assert(serializedObject);
    assert(serializedObject->needs_update() && serializedObject->needs_update()->Length() > 0);

    memcpy(mNeedsUpdate.data(), serializedObject->needs_update()->data(), serializedObject->needs_update()->size() * sizeof(uint8_t));

    mHasConvolution = (serializedObject->energy_fields() != nullptr);
    mHasParametric = (serializedObject->reverbs() != nullptr);

    if (mHasConvolution)
    {
        mEnergyFields.resize(numProbes);

        for (auto i = 0; i < numProbes; ++i)
        {
            if (!mNeedsUpdate[i])
            {
                if (serializedObject->energy_fields()->Length() > i && serializedObject->energy_fields()->Get(i) != nullptr)
                {
                    mEnergyFields[i] = ipl::make_unique<EnergyField>(serializedObject->energy_fields()->Get(i));
                }
                else
                {
                    mEnergyFields[i] = nullptr;
                }
            }
        }
    }

    if (mHasParametric)
    {
        mReverbs.resize(numProbes);
        memcpy(mReverbs.data(), serializedObject->reverbs()->data(), serializedObject->reverbs()->size() * sizeof(Reverb));
    }
}

void BakedReflectionsData::updateProbePosition(int index,
                                               const Vector3f& position)
{
    mNeedsUpdate[index] = true;
}

void BakedReflectionsData::addProbe(const Sphere& influence)
{
    mNeedsUpdate.push_back(true);

    if (mHasConvolution)
    {
        mEnergyFields.push_back(nullptr);
    }

    if (mHasParametric)
    {
        mReverbs.push_back(Reverb{});
    }
}

void BakedReflectionsData::removeProbe(int index)
{
    mNeedsUpdate.erase(mNeedsUpdate.begin() + index);

    if (mHasConvolution)
    {
        mEnergyFields.erase(mEnergyFields.begin() + index);
    }

    if (mHasParametric)
    {
        mReverbs.erase(mReverbs.begin() + index);
    }
}

void BakedReflectionsData::updateEndpoint(const BakedDataIdentifier& identifier,
                                          const Probe* probes,
                                          const Sphere& endpointInfluence)
{
    if (identifier != mIdentifier)
        return;

    if (mIdentifier.endpointInfluence.center != endpointInfluence.center)
    {
        for (auto i = 0u; i < mEnergyFields.size(); ++i)
        {
            mNeedsUpdate[i] = true;
        }
    }
    else
    {
        for (auto i = 0u; i < mEnergyFields.size(); ++i)
        {
            auto isInside = endpointInfluence.contains(probes[i].influence.center);
            auto wasInside = mIdentifier.endpointInfluence.contains(probes[i].influence.center);

            if (isInside && !wasInside)
            {
                mNeedsUpdate[i] = true;
            }
            else if (!isInside && wasInside)
            {
                if (mHasConvolution)
                {
                    mEnergyFields[i] = nullptr;
                }

                if (mHasParametric)
                {
                    mReverbs[i] = Reverb{};
                }
            }
        }
    }
}

uint64_t BakedReflectionsData::serializedSize() const
{
    uint64_t size = (mNeedsUpdate.size() * sizeof(uint8_t) +
                     2 * sizeof(bool));

    if (mHasConvolution)
    {
        for (auto i = 0u; i < mEnergyFields.size(); ++i)
        {
            if (!mNeedsUpdate[i])
            {
                size += sizeof(bool);

                if (mEnergyFields[i])
                {
                    size += mEnergyFields[i]->serializedSize();
                }
            }
        }
    }

    if (mHasParametric)
    {
        for (auto i = 0u; i < mReverbs.size(); ++i)
        {
            if (!mNeedsUpdate[i])
            {
                size += sizeof(Reverb);
            }
        }
    }

    return size;
}

void BakedReflectionsData::evaluateEnergyField(const ProbeNeighborhood& neighborhood, EnergyField& energyField)
{
    for (auto i = 0; i < neighborhood.numProbes(); ++i)
    {
        if (!neighborhood.batches[i] || neighborhood.probeIndices[i] < 0)
            continue;

        if (neighborhood.batches[i]->hasData(mIdentifier) && &(*neighborhood.batches[i])[mIdentifier] != this)
            continue;

        auto* probeEnergyField = lookupEnergyField(neighborhood.probeIndices[i]);
        if (!probeEnergyField)
            continue;

        EnergyField::scaleAccumulate(*probeEnergyField, neighborhood.weights[i], energyField);
    }
}

void BakedReflectionsData::evaluateReverb(const ProbeNeighborhood& neighborhood, Reverb& reverb)
{
    for (auto i = 0; i < neighborhood.numProbes(); ++i)
    {
        if (!neighborhood.batches[i] || neighborhood.probeIndices[i] < 0)
            continue;

        if (neighborhood.batches[i]->hasData(mIdentifier) && &(*neighborhood.batches[i])[mIdentifier] != this)
            continue;

        auto* probeReverb = lookupReverb(neighborhood.probeIndices[i]);
        if (!probeReverb)
            continue;

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            reverb.reverbTimes[j] += neighborhood.weights[i] * probeReverb->reverbTimes[j];
        }
    }
}

flatbuffers::Offset<Serialized::BakedReflectionsData> BakedReflectionsData::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> needsUpdateOffset = 0;
    needsUpdateOffset = fbb.CreateVector(mNeedsUpdate.data(), mNeedsUpdate.size());

    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Serialized::EnergyField>>> energyFieldsOffset = 0;
    if (mHasConvolution)
    {
        vector<flatbuffers::Offset<Serialized::EnergyField>> energyFieldOffsets(mEnergyFields.size());

        for (auto i = 0u; i < mEnergyFields.size(); ++i)
        {
            energyFieldOffsets[i] = (!mNeedsUpdate[i] && mEnergyFields[i]) ? mEnergyFields[i]->serialize(serializedObject) : 0;
        }

        energyFieldsOffset = fbb.CreateVector(energyFieldOffsets.data(), energyFieldOffsets.size());
    }

    flatbuffers::Offset<flatbuffers::Vector<const Serialized::Reverb*>> reverbsOffset = 0;
    if (mHasParametric)
    {
        reverbsOffset = fbb.CreateVectorOfStructs(reinterpret_cast<const Serialized::Reverb*>(mReverbs.data()), mReverbs.size());
    }

    return Serialized::CreateBakedReflectionsData(fbb, energyFieldsOffset, reverbsOffset, needsUpdateOffset);
}

int BakedReflectionsData::numProbes() const
{
    return static_cast<int>(mNeedsUpdate.size());
}

void BakedReflectionsData::setHasConvolution(bool hasConvolution)
{
    if (!mHasConvolution && hasConvolution)
    {
        mEnergyFields.resize(mNeedsUpdate.size());

        for (auto i = 0u; i < mNeedsUpdate.size(); ++i)
        {
            mNeedsUpdate[i] = true;
        }
    }
}

void BakedReflectionsData::setHasParametric(bool hasParametric)
{
    if (!mHasParametric && hasParametric)
    {
        mReverbs.resize(mNeedsUpdate.size());

        for (auto i = 0u; i < mNeedsUpdate.size(); ++i)
        {
            mNeedsUpdate[i] = true;
        }
    }
}

bool BakedReflectionsData::needsUpdate(int index) const
{
    return (mNeedsUpdate[index] != 0);
}

void BakedReflectionsData::set(int index,
                               unique_ptr<EnergyField> value)
{
    mEnergyFields[index] = std::move(value);
    mNeedsUpdate[index] = false;
}

void BakedReflectionsData::set(int index,
                               const Reverb& value)
{
    mReverbs[index] = value;
    mNeedsUpdate[index] = false;
}

EnergyField* BakedReflectionsData::lookupEnergyField(int index)
{
    return (mHasConvolution) ? mEnergyFields[index].get() : nullptr;
}

Reverb* BakedReflectionsData::lookupReverb(int index)
{
    return (mHasParametric) ? &mReverbs[index] : nullptr;
}

}
