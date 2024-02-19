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

#include "probe_data.h"
#include "probe_generator.h"
#include "probe_tree.h"

#include "probe_batch.fbs.h"

namespace ipl {

class ProbeBatch;

// ---------------------------------------------------------------------------------------------------------------------
// ProbeNeighborhood
// ---------------------------------------------------------------------------------------------------------------------

class ProbeNeighborhood
{
public:
    static const int kMaxProbesPerBatch = 8;

    Array<ProbeBatch*> batches;
    Array<int> probeIndices;
    Array<float> weights;

    // Buffers for occlusion checks
    Array<Ray> rays;
    Array<float> minDistances;
    Array<float> maxDistances;
    Array<int> rayMapping;
    Array<bool> isOccluded;

    int numProbes() const
    {
        return static_cast<int>(weights.size(0));
    }

    int numValidProbes() const
    {
        auto count = 0;
        for (auto i = 0; i < numProbes(); ++i)
        {
            if (batches[i] && probeIndices[i] >= 0)
            {
                ++count;
            }
        }

        return count;
    }

    bool hasValidProbes() const
    {
        for (auto i = 0; i < numProbes(); ++i)
        {
            if (batches[i] && probeIndices[i] >= 0)
                return true;
        }

        return false;
    }

    void resize(int maxProbes);

    void reset();

    void checkOcclusion(const IScene& scene,
        const Vector3f& point);

    int findNearest(const Vector3f& point) const;

    void calcWeights(const Vector3f& point);
};


// ---------------------------------------------------------------------------------------------------------------------
// ProbeBatch
// ---------------------------------------------------------------------------------------------------------------------

class ProbeBatch
{
public:
    ProbeBatch()
    {}

    ProbeBatch(const Serialized::ProbeBatch* serializedObject);

    ProbeBatch(SerializedObject& serializedObject);

    int numProbes() const
    {
        return static_cast<int>(mProbes.size());
    }

    Probe* probes()
    {
        return mProbes.data();
    }

    const Probe* probes() const
    {
        return mProbes.data();
    }

    Probe& operator[](int i)
    {
        return mProbes[i];
    }

    const Probe& operator[](int i) const
    {
        return mProbes[i];
    }

    bool hasData(const BakedDataIdentifier& identifier) const
    {
        return (mData.find(identifier) != mData.end());
    }

    IBakedData& operator[](const BakedDataIdentifier& identifier)
    {
        return *mData[identifier];
    }

    const IBakedData& operator[](const BakedDataIdentifier& identifier) const
    {
        return *mData.at(identifier);
    }

    void toProbeArray(ProbeArray& probeArray) const;

    void updateProbeRadius(int index,
                           float radius);

    void updateProbePosition(int index,
                             const Vector3f& position);

    void addProbe(const Sphere& influence);

    void addProbeArray(const ProbeArray& probeArray);

    void removeProbe(int index);

    void updateEndpoint(const BakedDataIdentifier& identifier,
                        const Sphere& endpointInfluence);

    void commit();

    void addData(const BakedDataIdentifier& identifier,
                 unique_ptr<IBakedData> data);

    void removeData(const BakedDataIdentifier& identifier);

    void getInfluencingProbes(const Vector3f& point,
                              ProbeNeighborhood& neighborhood,
                              int offset = 0);

    flatbuffers::Offset<Serialized::ProbeBatch> serialize(SerializedObject& serializedObject) const;

    void serializeAsRoot(SerializedObject& serializedObject) const;

private:
    vector<Probe> mProbes;
    unique_ptr<ProbeTree> mProbeTree;
    map<BakedDataIdentifier, unique_ptr<IBakedData>> mData;
};

}
