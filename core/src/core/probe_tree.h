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

#include "box.h"
#include "probe.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ProbeTreeNode
// --------------------------------------------------------------------------------------------------------------------

struct ProbeTreeNode
{
    int32_t data() const
    {
        return reinterpret_cast<const int32_t*>(&box.minCoordinates)[3];
    }

    int32_t getSplitAxis() const
    {
        return data() & 3;
    }

    int32_t getProbeIndex() const
    {
        return data() >> 2;
    }

    ProbeTreeNode const& getLeftChild() const
    {
        return this[data() >> 2];
    }

    ProbeTreeNode const& getRightChild() const
    {
        return this[(data() >> 2) + 1];
    }

    float getSplitCoordinate() const
    {
        return reinterpret_cast<float const*>(&box.maxCoordinates)[3];
    }

    bool isLeaf() const
    {
        return (getSplitAxis() == 3);
    }

    void setData(int32_t data)
    {
        reinterpret_cast<int32_t*>(&box.minCoordinates)[3] = data;
    }

    void setProbeIndex(int32_t probeIndex)
    {
        setData((probeIndex << 2) | 3);
    }

    void setInternalNodeData(int32_t childOffset,
                             int32_t splitAxis)
    {
        setData((childOffset << 2) | splitAxis);
    }

    void setSplitCoordinate(float splitCoordinate)
    {
        reinterpret_cast<float*>(&box.maxCoordinates)[3] = splitCoordinate;
    }

    Box box;
};


// --------------------------------------------------------------------------------------------------------------------
// ProbeTree
// --------------------------------------------------------------------------------------------------------------------

class ProbeTree
{
public:
    ProbeTree(int numProbes,
              const Probe* probes);

    const ProbeTreeNode& getRootNode() const { return mNodes[0]; }

    void getInfluencingProbes(const Vector3f& point,
                              const Probe* probes,
                              int maxInfluencingProbes,
                              int* probeIndices);

private:
    static const int kProbeLookupStackSize;

    Array<ProbeTreeNode> mNodes;
};

}