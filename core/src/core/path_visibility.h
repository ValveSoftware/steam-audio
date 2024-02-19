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

#include "probe_batch.h"
#include "scene.h"

#include "path_visibility.fbs.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ProbeVisibilityTester
// --------------------------------------------------------------------------------------------------------------------

// Tests whether two probes are mutually visible.
class ProbeVisibilityTester
{
public:
    // Creates a visibility tester that uses a given number of ray samples for testing. If the number of samples is
    // 1 or fewer, point-to-point visibility is used. If the number of samples is greater than 1, volumetric visibility
    // is used.
    ProbeVisibilityTester(int numSamples,
                          bool asymmetricVisRange,
                          const Vector3f& down);

    // Tests whether two probes are mutually visible.
    bool areProbesVisible(const IScene& scene,
                          const ProbeBatch& probes,
                          int from,
                          int to,
                          float radius,
                          float threshold) const;

    // Tests whether two probes are farther apart than a given range.
    bool areProbesTooFar(const ProbeBatch& probes,
                         int from,
                         int to,
                         float visRange) const;

private:
    Array<Vector3f> mSamples; // Point samples used for visibility checks.
    bool mAsymmetricVisRange;
    Vector3f mDown;
};


// --------------------------------------------------------------------------------------------------------------------
// ProbeVisibilityGraph
// --------------------------------------------------------------------------------------------------------------------

// A graph describing visibility between probes. Each node in the graph is a probe; an (undirected) edge exists between
// two nodes if they are mutually visible.
class ProbeVisibilityGraph
{
public:
    vector<list<int>> mAdjacent; // The graph, represented as an adjacency list.

    // Computes a visibility graph given an array of probes (more precisely, pointers to probes).
    ProbeVisibilityGraph(const IScene& scene,
                         const ProbeBatch& probes,
                         const ProbeVisibilityTester& visTester,
                         float radius,
                         float threshold,
                         float visRange,
                         int numThreads,
                         std::atomic<bool>& cancel,
                         ProgressCallback progressCallback = nullptr,
                         void* callbackUserData = nullptr);

    // Deserializes a visibility graph.
    ProbeVisibilityGraph(const Serialized::VisibilityGraph* serializedObject);

    // Tests whether an edge exists between two probes, i.e., whether the graph indicates that the two probes are
    // mutually visible.
    bool hasEdge(int from,
                 int to) const;

    // Removes all edges in the graph between probes that are further apart than a given range.
    void prune(const ProbeBatch& probes,
               const ProbeVisibilityTester& visTester,
               float visRange);

    // Returns the size of a serialized representation of this object.
    uint64_t serializedSize() const;

    // Serializes this object.
    flatbuffers::Offset<Serialized::VisibilityGraph> serialize(SerializedObject& serializedObject) const;
};

}
