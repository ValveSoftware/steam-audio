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

#include "path_visibility.h"

#include "job_graph.h"
#include "sampling.h"
#include "sh.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ProbeVisibilityTester
// --------------------------------------------------------------------------------------------------------------------

ProbeVisibilityTester::ProbeVisibilityTester(int numSamples,
                                             bool asymmetricVisRange,
                                             const Vector3f& down)
    : mAsymmetricVisRange(asymmetricVisRange)
    , mDown(down)
{
    if (numSamples > 1)
    {
        mSamples.resize(numSamples);
        Sampling::generateSphereVolumeSamples(numSamples, mSamples.data());
    }
}

// To determine mutual visibility between probes A and B, they are considered as spheres of a given radius. A
// set of numSamples points are generated in A and B, and rays are traced between each pair (i.e., O(numSamples^2) rays
// are traced).
//
// Point-to-point visibility can be specified by setting numSamples to 1 or radius to 0.
//
// If the fraction of unoccluded rays is at least threshold, the probes are considered mutually visible.
bool ProbeVisibilityTester::areProbesVisible(const IScene& scene,
                                             const ProbeBatch& probes,
                                             int from,
                                             int to,
                                             float radius,
                                             float threshold) const
{
    auto fromProbe = probes[from].influence.center;
    auto toProbe = probes[to].influence.center;

    if (mSamples.size(0) > 0 && radius > 0.0f)
    {
        auto numVisibleSamples = 0;

        for (auto i = 0u; i < mSamples.size(0); ++i)
        {
            auto fromSample = Sampling::transformSphereVolumeSample(mSamples[i], Sphere(fromProbe, radius));
            if (scene.isOccluded(fromProbe, fromSample))
                continue;

            for (auto j = 0u; j < mSamples.size(0); ++j)
            {
                auto toSample = Sampling::transformSphereVolumeSample(mSamples[j], Sphere(toProbe, radius));
                if (scene.isOccluded(toProbe, toSample))
                    continue;

                if (!scene.isOccluded(fromSample, toSample))
                {
                    ++numVisibleSamples;
                    if ((static_cast<float>(numVisibleSamples) / static_cast<float>(mSamples.size(0))) >= threshold)
                        return true;
                }
            }
        }

        return false;
    }
    else
    {
        return (!scene.isOccluded(fromProbe, toProbe));
    }
}

// To save time, all pairs of probes whose distance from each other is at least visRange can be considered mutually
// invisible.
bool ProbeVisibilityTester::areProbesTooFar(const ProbeBatch& probes,
                                            int from,
                                            int to,
                                            float visRange) const
{
    auto d = Vector3f(probes[from].influence.center - probes[to].influence.center);

    if (mAsymmetricVisRange)
    {
        d -= Vector3f::dot(d, mDown) * mDown;
    }

    return (d.length() > visRange);
}


// --------------------------------------------------------------------------------------------------------------------
// ProbeVisibilityGraph
// --------------------------------------------------------------------------------------------------------------------

ProbeVisibilityGraph::ProbeVisibilityGraph(const IScene& scene,
                                           const ProbeBatch& probes,
                                           const ProbeVisibilityTester& visTester,
                                           float radius,
                                           float threshold,
                                           float visRange,
                                           int numThreads,
                                           JobGraph& jobGraph,
                                           std::atomic<bool>& cancel,
                                           ProgressCallback progressCallback,
                                           void* callbackUserData)
    : mAdjacent(probes.numProbes())
    , mNumJobsRemaining(0)
{
    PROFILE_FUNCTION();

    // For any 2 probe indices (i, j), we will only check visibility if i > j.
    // We will divide the work of constructing the visibility graph into a set of jobs, where each job involves
    // constructing one or more rows of the adjacency list (mAdjacent[i]). A given row will never be processed by
    // multiple threads concurrently.
    auto numProbes = probes.numProbes();
    auto numProbesPerJob = 1;

    auto numProbesThisJob = 0;
    auto firstProbeThisJob = 0;
    for (auto i = 0; i < numProbes; i++)
    {
        numProbesThisJob++;
        if (numProbesThisJob == 1)
        {
            firstProbeThisJob = i;
        }

        if (numProbesThisJob == numProbesPerJob ||
            i == numProbes - 1)
        {
            jobGraph.addJob([this, firstProbeThisJob, numProbesThisJob, radius, threshold, visRange, &scene, &probes, &visTester](int threadIndex, std::atomic<bool>& cancel)
            {
                for (auto i = firstProbeThisJob; i < firstProbeThisJob + numProbesThisJob; i++)
                {
                    for (auto j = 0; j < i; ++j)
                    {
                        if (visTester.areProbesTooFar(probes, i, j, visRange))
                            continue;

                        if (!visTester.areProbesVisible(scene, probes, i, j, radius, threshold))
                            continue;

                        auto cost = (probes[i].influence.center - probes[j].influence.center).length();

                        mAdjacent[i].push_back(AdjacencyListEntry{j, cost});
                    }
                }

                // The last job we process "completes" the adjacency list, by making sure that if we have an edge from
                // i to j, we also have an edge from j to i.
                if (std::atomic_fetch_sub_explicit(&mNumJobsRemaining, 1, std::memory_order_seq_cst) == 1)
                {
                    for (auto i = 0; i < probes.numProbes(); i++)
                    {
                        for (const auto& entry : mAdjacent[i])
                        {
                            mAdjacent[entry.index].push_back(AdjacencyListEntry{i, entry.cost});
                        }
                    }
                }
            });

            mNumJobsRemaining++;
            numProbesThisJob = 0;
        }
    }
}

ProbeVisibilityGraph::ProbeVisibilityGraph(const Serialized::VisibilityGraph* serializedObject)
    : mNumJobsRemaining(0)
{
    PROFILE_FUNCTION();

    assert(serializedObject);
    assert(serializedObject->nodes() && serializedObject->nodes()->Length() > 0);

    auto numProbes = serializedObject->nodes()->Length();

    mAdjacent.resize(numProbes);

    for (auto i = 0u; i < numProbes; ++i)
    {
        auto numEdges = serializedObject->nodes()->Get(i)->edges()->Length();

        for (auto j = 0u; j < numEdges; ++j)
        {
            auto edge = serializedObject->nodes()->Get(i)->edges()->Get(j);
            mAdjacent[i].push_back(AdjacencyListEntry{edge, 0.0f});
        }
    }

    for (auto i = 0; i < static_cast<int>(numProbes); ++i)
    {
        for (const auto& entry : mAdjacent[i])
        {
            auto j = entry.index;
            if (j < i)
            {
                mAdjacent[j].push_back(AdjacencyListEntry{i, 0.0f});
            }
        }
    }
}

void ProbeVisibilityGraph::updateCosts(const ProbeBatch& probeBatch)
{
    for (auto i = 0; i < mAdjacent.size(); i++)
    {
        for (auto& entry : mAdjacent[i])
        {
            auto j = entry.index;
            entry.cost = (probeBatch[i].influence.center - probeBatch[j].influence.center).length();
        }
    }
}

bool ProbeVisibilityGraph::hasEdge(int from,
                                   int to) const
{
    return (std::find_if(mAdjacent[from].begin(), mAdjacent[from].end(), [to](const AdjacencyListEntry& value) { return value.index == to; }) != mAdjacent[from].end());
}

void ProbeVisibilityGraph::prune(const ProbeBatch& probes,
                                 const ProbeVisibilityTester& visTester,
                                 float visRange)
{
    for (auto i = 0u; i < mAdjacent.size(); ++i)
    {
        auto isProbeTooFar = [&probes, &visTester, visRange, i](const AdjacencyListEntry& j)
        {
            return visTester.areProbesTooFar(probes, i, j.index, visRange);
        };

        mAdjacent[i].erase(std::remove_if(mAdjacent[i].begin(), mAdjacent[i].end(), isProbeTooFar), mAdjacent[i].end());
    }
}

uint64_t ProbeVisibilityGraph::serializedSize() const
{
    uint64_t size = sizeof(int32_t);

    auto numProbes = static_cast<int>(mAdjacent.size());
    for (auto i = 0; i < numProbes; ++i)
    {
        auto numEdges = 0;
        for (const auto& entry : mAdjacent[i])
        {
            auto j = entry.index;
            if (j < i)
            {
                ++numEdges;
            }
        }

        size += sizeof(int32_t) + numEdges * sizeof(int32_t);
    }

    return size;
}

flatbuffers::Offset<Serialized::VisibilityGraph> ProbeVisibilityGraph::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    auto numProbes = static_cast<int>(mAdjacent.size());
    vector<flatbuffers::Offset<Serialized::VisibilityList>> visibilityListOffsets(numProbes);

    for (auto i = 0; i < numProbes; ++i)
    {
        auto numEdges = 0;
        for (const auto& entry : mAdjacent[i])
        {
            auto j = entry.index;
            if (j < i)
            {
                ++numEdges;
            }
        }

        vector<int32_t> edges;
        edges.reserve(numEdges);
        for (const auto& entry : mAdjacent[i])
        {
            auto j = entry.index;
            if (j < i)
            {
                edges.push_back(j);
            }
        }

        auto edgesOffset = fbb.CreateVector(edges.data(), edges.size());

        visibilityListOffsets[i] = Serialized::CreateVisibilityList(fbb, edgesOffset);
    }

    auto visibilityListsOffset = fbb.CreateVector(visibilityListOffsets.data(), visibilityListOffsets.size());

    return Serialized::CreateVisibilityGraph(fbb, visibilityListsOffset);
}

}
