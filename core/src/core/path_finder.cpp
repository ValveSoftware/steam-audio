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

#include "path_finder.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// PathFinder
// --------------------------------------------------------------------------------------------------------------------

PathFinder::PathFinder(const ProbeBatch& probes,
                       int numThreads)
    : mParents(numThreads, probes.numProbes())
    , mCosts(numThreads, probes.numProbes())
{
    for (auto i = 0; i < numThreads; ++i)
    {
        vector<PriorityQueueEntry> container;
        container.reserve(2 * probes.numProbes());

        priority_queue<PriorityQueueEntry> priorityQueue(std::less<PriorityQueueEntry>(), std::move(container));

        mPriorityQueue.push_back(priorityQueue);
    }
}

// Uses Dijkstra's algorithm to find the minimum spanning tree rooted at the start node.
void PathFinder::findAllShortestPaths(const IScene& scene,
                                      const ProbeBatch& probes,
                                      const ProbeVisibilityGraph& visGraph,
                                      int start,
                                      float radius,
                                      float threshold,
                                      float pathRange,
                                      int threadIndex,
                                      ProbePath* paths) const
{
    PROFILE_FUNCTION();

    for (auto i = 0; i < probes.numProbes(); ++i)
    {
        mParents[threadIndex][i] = -1;
        mCosts[threadIndex][i] = std::numeric_limits<float>::infinity();
    }

    mCosts[threadIndex][start] = 0.0f;

    while (!mPriorityQueue[threadIndex].empty())
    {
        mPriorityQueue[threadIndex].pop();
    }

    mPriorityQueue[threadIndex].push(PriorityQueueEntry{start, mCosts[threadIndex][start]});

    while (!mPriorityQueue[threadIndex].empty())
    {
        auto u = mPriorityQueue[threadIndex].top().nodeIndex;

        mPriorityQueue[threadIndex].pop();

        for (auto v : visGraph.mAdjacent[u])
        {
            auto uvDistance = (probes[u].influence.center - probes[v].influence.center).length();

            if (mCosts[threadIndex][u] + uvDistance < mCosts[threadIndex][v])
            {
                mCosts[threadIndex][v] = mCosts[threadIndex][u] + uvDistance;
                mParents[threadIndex][v] = u;

                mPriorityQueue[threadIndex].push(PriorityQueueEntry{v, mCosts[threadIndex][v]});
            }
        }
    }

    for (auto i = 0; i < probes.numProbes(); ++i)
    {
        paths[i].start = start;
        paths[i].end = i;

        if ((probes[start].influence.center - probes[i].influence.center).length() > pathRange)
        {
            paths[i].valid = false;
            continue;
        }

        if (mParents[threadIndex][i] >= 0)
        {
            paths[i].valid = true;

            auto parentIndex = mParents[threadIndex][i];
            while (parentIndex >= 0 && parentIndex != start)
            {
                paths[i].nodes.insert(paths[i].nodes.begin(), parentIndex);
                parentIndex = mParents[threadIndex][parentIndex];
            }
        }
        else
        {
            paths[i].valid = false;
        }
    }

    for (auto i = 0; i < probes.numProbes(); ++i)
    {
        if (!paths[i].valid)
            continue;

        bool pathValid = true;
        for (auto j = 0; j < paths[i].nodes.size() && pathValid; ++j)
        {
            float dist = (probes[paths[i].start].influence.center - probes[paths[i].nodes[j]].influence.center).length();
            pathValid = pathValid && (dist <= pathRange);
        }

        for (int j = static_cast<int>(paths[i].nodes.size()) - 1; j >= 0 && pathValid; --j)
        {
            float dist = (probes[paths[i].end].influence.center - probes[paths[i].nodes[j]].influence.center).length();
            pathValid = pathValid && (dist <= pathRange);
        }

        if (!pathValid)
        {
            paths[i].reset();
        }
    }
}

// Uses A* to speed up processing.
ProbePath PathFinder::findShortestPath(const IScene& scene,
                                       const ProbeBatch& probes,
                                       const ProbeVisibilityGraph& visGraph,
                                       const ProbeVisibilityTester& visTester,
                                       int start,
                                       int end,
                                       float radius,
                                       float threshold,
                                       float visRange,
                                       bool simplifyPaths,
                                       bool realTimeVis,
                                       int threadIndex) const
{
    PROFILE_FUNCTION();

    ProbePath result;
    result.start = start;
    result.end = end;

    for (auto i = 0; i < probes.numProbes(); ++i)
    {
        mParents[threadIndex][i] = -1;
        mCosts[threadIndex][i] = std::numeric_limits<float>::infinity();
    }

    auto ProbeDistance = [&probes](int start, int end) -> float
        {
            auto d = Vector3f(probes[start].influence.center - probes[end].influence.center);
            auto dx = fabsf(d.x());
            auto dy = fabsf(d.y());
            auto dz = fabsf(d.z());

            return (dx + dy + dz);
        };

    mCosts[threadIndex][start] = ProbeDistance(start, end);

    while (!mPriorityQueue[threadIndex].empty())
    {
        mPriorityQueue[threadIndex].pop();
    }

    mPriorityQueue[threadIndex].push(PriorityQueueEntry{start, mCosts[threadIndex][start]});

    while (!mPriorityQueue[threadIndex].empty())
    {
        auto u = mPriorityQueue[threadIndex].top().nodeIndex;

        if (u == end)
            break;

        mPriorityQueue[threadIndex].pop();

        for (auto v : visGraph.mAdjacent[u])
        {
            auto uvDistance = ProbeDistance(u, v);
            auto uEndDistance = ProbeDistance(u, end);
            auto vEndDistance = ProbeDistance(v, end);

            if ((mCosts[threadIndex][u] + uvDistance - uEndDistance + vEndDistance) < mCosts[threadIndex][v])
            {
                if (realTimeVis)
                {
                    if (!visTester.areProbesVisible(scene, probes, u, v, radius, threshold))
                        continue;
                }

                mCosts[threadIndex][v] = mCosts[threadIndex][u] + uvDistance - uEndDistance + vEndDistance;
                mParents[threadIndex][v] = u;

                mPriorityQueue[threadIndex].push(PriorityQueueEntry{v, mCosts[threadIndex][v]});
            }
        }
    }

    if (mParents[threadIndex][end] < 0)
        return result;

    if (simplifyPaths)
    {
        simplifyPath(scene, probes, visGraph, visTester, start, end, radius, threshold, realTimeVis, mParents[threadIndex]);
    }

    auto parentIndex = mParents[threadIndex][end];
    while (parentIndex >= 0)
    {
        result.nodes.insert(result.nodes.begin(), parentIndex);
        parentIndex = mParents[threadIndex][parentIndex];
    }

    result.valid = true;
    return result;
}

void PathFinder::simplifyPath(const IScene& scene,
                              const ProbeBatch& probes,
                              const ProbeVisibilityGraph& visGraph,
                              const ProbeVisibilityTester& visTester,
                              int start,
                              int end,
                              float radius,
                              float threshold,
                              bool realTimeVis,
                              int* parents) const
{
    PROFILE_FUNCTION();

    auto current = end;
    while (current != start && current >= 0)
    {
        while (true)
        {
            auto parent = parents[current];
            if (parent < 0)
                break;

            auto grandparent = parents[parent];
            if (grandparent < 0)
                break;

            if (realTimeVis)
            {
                if (!visTester.areProbesVisible(scene, probes, current, grandparent, radius, threshold))
                    break;
            }
            else
            {
                if (!visGraph.hasEdge(current, grandparent))
                    break;
            }

            parents[current] = grandparent;
        }

        current = parents[current];
    }
}

bool operator<(const PathFinder::PriorityQueueEntry& lhs,
               const PathFinder::PriorityQueueEntry& rhs)
{
    return (lhs.cost > rhs.cost);
}

}
