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

#include "probe_tree.h"

#include <numeric>

#include "bvh.h"
#include "profiler.h"
#include "stack.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ProbeTreeConstructionTask
// --------------------------------------------------------------------------------------------------------------------

struct ProbeTreeConstructionTask
{
    int32_t nodeIndex;
    int32_t startIndex;
    int32_t endIndex;
    int32_t leftChildIndex;
};


// --------------------------------------------------------------------------------------------------------------------
// ProbeTree
// --------------------------------------------------------------------------------------------------------------------

const int ProbeTree::kProbeLookupStackSize = 128;

ProbeTree::ProbeTree(int numProbes,
                     const Probe* probes)
{
    if (numProbes <= 0)
        return;

    mNodes.resize(2 * numProbes - 1);

    Array<int32_t> leafIndices(numProbes);
    Array<Box> leafBounds(numProbes);
    Array<Vector3f> leafBoxCenters(numProbes);
    for (auto i = 0; i < numProbes; ++i)
    {
        leafIndices[i] = i;

        const auto& sphere = probes[i].influence;
        auto delta = sphere.radius * Vector3f(1, 1, 1);

        leafBounds[i] = Box(sphere.center - delta, sphere.center + delta);

        leafBoxCenters[i] = sphere.center;
    }

    Array<CentroidCoordinate, 2> centroids(3, numProbes);

    Stack<ProbeTreeConstructionTask, kProbeLookupStackSize> stack;
    ProbeTreeConstructionTask task{0, 0, numProbes - 1, 1};

    while (true)
    {
        if (task.startIndex == task.endIndex)
        {
            mNodes[task.nodeIndex].box = leafBounds[leafIndices[task.startIndex]];
            mNodes[task.nodeIndex].setProbeIndex(leafIndices[task.startIndex]);

            if (stack.isEmpty())
                break;

            task = stack.pop();
        }
        else
        {
            alignas(float4_t) GrowableBox nodeBounds;
            for (auto i = task.startIndex; i <= task.endIndex; ++i)
            {
                alignas(float4_t) GrowableBox bounds;
                bounds.load(leafBounds[leafIndices[i]]);
                nodeBounds.growToContain(bounds);
            }
            nodeBounds.store(mNodes[task.nodeIndex].box);

            for (auto i = task.startIndex; i <= task.endIndex; ++i)
            {
                centroids[0][i].coordinate = leafBoxCenters[leafIndices[i]].x();
                centroids[1][i].coordinate = leafBoxCenters[leafIndices[i]].y();
                centroids[2][i].coordinate = leafBoxCenters[leafIndices[i]].z();
                centroids[0][i].leafIndex = leafIndices[i];
                centroids[1][i].leafIndex = leafIndices[i];
                centroids[2][i].leafIndex = leafIndices[i];
            }

            auto splitIndex = (task.endIndex - task.startIndex + 1) / 2;
            auto splitAxis = mNodes[task.nodeIndex].box.extents().indexOfMaxComponent();

            auto compareCentroids = [](const CentroidCoordinate& lhs,
                                       const CentroidCoordinate& rhs)
            {
                return lhs.coordinate < rhs.coordinate;
            };

            std::sort(&centroids[splitAxis][task.startIndex], &centroids[splitAxis][task.endIndex + 1], compareCentroids);

            for (auto i = task.startIndex; i <= task.endIndex; ++i)
            {
                leafIndices[i] = centroids[splitAxis][i].leafIndex;
            }

            auto splitCoordinate = centroids[splitAxis][task.startIndex + splitIndex].coordinate;

            mNodes[task.nodeIndex].setInternalNodeData(task.leftChildIndex - task.nodeIndex, splitAxis);
            mNodes[task.nodeIndex].setSplitCoordinate(splitCoordinate);

            stack.push(ProbeTreeConstructionTask{task.leftChildIndex + 1, task.startIndex + splitIndex, task.endIndex, task.leftChildIndex + 2 * splitIndex});
            task = ProbeTreeConstructionTask{task.leftChildIndex, task.startIndex, task.startIndex + splitIndex - 1, task.leftChildIndex + 2};
            continue;
        }
    }
}

void ProbeTree::getInfluencingProbes(const Vector3f& point,
                                     const Probe* probes,
                                     int maxInfluencingProbes,
                                     int* probeIndices)
{
    PROFILE_FUNCTION();

    for (auto i = 0; i < maxInfluencingProbes; ++i)
    {
        probeIndices[i] = -1;
    }

    auto numInfluencingProbes = 0;

    Stack<const ProbeTreeNode*, kProbeLookupStackSize> stack;
    const auto* node = &mNodes[0];

    while (true)
    {
        if (node->box.contains(point))
        {
            if (node->isLeaf())
            {
                if (probes[node->getProbeIndex()].influence.contains(point))
                {
                    probeIndices[numInfluencingProbes] = node->getProbeIndex();
                    ++numInfluencingProbes;
                    if (numInfluencingProbes >= maxInfluencingProbes)
                        break;
                }
            }
            else
            {
                auto nearChild = &node->getLeftChild();
                auto farChild = &node->getRightChild();
                if (point.elements[node->getSplitAxis()] > node->getSplitCoordinate())
                {
                    std::swap(nearChild, farChild);
                }

                stack.push(farChild);
                node = nearChild;
                continue;
            }
        }

        if (stack.isEmpty())
            break;

        node = stack.pop();
    }
}

}
