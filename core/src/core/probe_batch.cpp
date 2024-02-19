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

#include "probe_batch.h"

#include "baked_reflection_data.h"
#include "path_data.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// ProbeBatch
// ---------------------------------------------------------------------------------------------------------------------

ProbeBatch::ProbeBatch(const Serialized::ProbeBatch* serializedObject)
{
    assert(serializedObject);
    assert(serializedObject->probes() && serializedObject->probes()->Length() > 0);

    auto numProbes = serializedObject->probes()->Length();

    mProbes.resize(numProbes);
    memcpy(mProbes.data(), serializedObject->probes()->data(), numProbes * sizeof(Probe));

    auto numDataLayers = serializedObject->data_layers() ? serializedObject->data_layers()->Length() : 0;

    for (auto i = 0u; i < numDataLayers; ++i)
    {
        BakedDataIdentifier identifier;
        identifier.variation = static_cast<BakedDataVariation>(serializedObject->data_layers()->Get(i)->identifier()->variation());
        identifier.type = static_cast<BakedDataType>(serializedObject->data_layers()->Get(i)->identifier()->type());
        identifier.endpointInfluence = *reinterpret_cast<const Sphere*>(serializedObject->data_layers()->Get(i)->identifier()->influence());

        unique_ptr<IBakedData> data;

        if (identifier.type == BakedDataType::Reflections)
        {
            data = ipl::make_unique<BakedReflectionsData>(identifier, numProbes, serializedObject->data_layers()->Get(i)->reflections_data());
        }
        else if (identifier.type == BakedDataType::Pathing)
        {
            data = ipl::make_unique<BakedPathData>(serializedObject->data_layers()->Get(i)->pathing_data());
        }

        addData(identifier, std::move(data));
    }
}

ProbeBatch::ProbeBatch(SerializedObject& serializedObject)
    : ProbeBatch(Serialized::GetProbeBatch(serializedObject.data()))
{}

void ProbeBatch::toProbeArray(ProbeArray& probeArray) const
{
    probeArray.probes.resize(mProbes.size());
    memcpy(probeArray.probes.data(), mProbes.data(), mProbes.size() * sizeof(Probe));
}

void ProbeBatch::updateProbeRadius(int index,
                                   float radius)
{
    mProbes[index].influence.radius = radius;
}

void ProbeBatch::updateProbePosition(int index,
                                     const Vector3f& position)
{
    mProbes[index].influence.center = position;

    for (auto& data : mData)
    {
        data.second->updateProbePosition(index, position);
    }
}

void ProbeBatch::addProbe(const Sphere& influence)
{
    Probe probe;
    probe.influence = influence;

    mProbes.push_back(probe);

    for (auto& data : mData)
    {
        data.second->addProbe(influence);
    }
}

void ProbeBatch::addProbeArray(const ProbeArray& probeArray)
{
    for (auto i = 0; i < probeArray.numProbes(); ++i)
    {
        addProbe(probeArray[i].influence);
    }
}

void ProbeBatch::removeProbe(int index)
{
    mProbes.erase(mProbes.begin() + index);

    for (auto& data : mData)
    {
        data.second->removeProbe(index);
    }
}

void ProbeBatch::updateEndpoint(const BakedDataIdentifier& identifier,
                                const Sphere& endpointInfluence)
{
    for (auto& data : mData)
    {
        data.second->updateEndpoint(identifier, mProbes.data(), endpointInfluence);
    }
}

void ProbeBatch::commit()
{
    mProbeTree = make_unique<ProbeTree>(static_cast<int>(mProbes.size()), mProbes.data());
}

void ProbeBatch::addData(const BakedDataIdentifier& identifier,
                         unique_ptr<IBakedData> data)
{
    mData[identifier] = std::move(data);
}

void ProbeBatch::removeData(const BakedDataIdentifier& identifier)
{
    mData.erase(identifier);
}

void ProbeBatch::getInfluencingProbes(const Vector3f& point,
                                      ProbeNeighborhood& neighborhood,
                                      int offset /* = 0 */)
{
    assert(mProbeTree);

    mProbeTree->getInfluencingProbes(point, mProbes.data(), ProbeNeighborhood::kMaxProbesPerBatch, &neighborhood.probeIndices[offset]);

    for (auto i = 0; i < ProbeNeighborhood::kMaxProbesPerBatch; ++i)
    {
        neighborhood.batches[offset + i] = this;
    }
}

flatbuffers::Offset<Serialized::ProbeBatch> ProbeBatch::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    vector<Sphere> probeSpheres(mProbes.size());
    for (auto i = 0u; i < mProbes.size(); ++i)
    {
        probeSpheres[i] = mProbes[i].influence;
    }

    auto probesOffset = fbb.CreateVectorOfStructs(reinterpret_cast<const Serialized::Sphere*>(probeSpheres.data()), probeSpheres.size());

    vector<flatbuffers::Offset<Serialized::BakedDataLayer>> dataLayerOffsets;
    dataLayerOffsets.reserve(mData.size());

    for (const auto& data : mData)
    {
        auto identifierOffset = Serialized::CreateBakedDataIdentifier(fbb,
                                                                      static_cast<Serialized::BakedDataVariation>(data.first.variation),
                                                                      static_cast<Serialized::BakedDataType>(data.first.type),
                                                                      reinterpret_cast<const Serialized::Sphere*>(&data.first.endpointInfluence));

        flatbuffers::Offset<Serialized::BakedReflectionsData> reflectionsDataOffset = 0;
        flatbuffers::Offset<Serialized::BakedPathingData> pathingDataOffset = 0;

        if (data.first.type == BakedDataType::Reflections)
        {
            reflectionsDataOffset = static_cast<BakedReflectionsData*>(data.second.get())->serialize(serializedObject);
        }
        else if (data.first.type == BakedDataType::Pathing)
        {
            pathingDataOffset = static_cast<BakedPathData*>(data.second.get())->serialize(serializedObject);
        }

        dataLayerOffsets.push_back(Serialized::CreateBakedDataLayer(fbb, identifierOffset, reflectionsDataOffset, pathingDataOffset));
    }

    auto dataLayersOffset = fbb.CreateVector(dataLayerOffsets.data(), dataLayerOffsets.size());

    return Serialized::CreateProbeBatch(fbb, probesOffset, dataLayersOffset);
}

void ProbeBatch::serializeAsRoot(SerializedObject& serializedObject) const
{
    serializedObject.fbb().Finish(serialize(serializedObject));
    serializedObject.commit();
}

}
