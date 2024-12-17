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

#include "path_visibility.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ProbePath
// --------------------------------------------------------------------------------------------------------------------

// A path from one probe to another, expressed as a sequence of probes. There are always at least 2 probes in a
// ProbePath: the first (start) probe, and the last (end) probe. There may be 0 or more probes in between. All probes
// are specified using indices into a probe array, which is typically passed in when computing the visibility graph
// or baking paths (see below).
class ProbePath
{
public:
    bool valid; // Is this path valid?
    int start; // Index of the first probe in the path.
    int end; // Index of the last probe in the path.
    vector<int> nodes; // Indices of probes between start and end, in order.

    ProbePath()
        : valid(false)
        , start(-1)
        , end(-1)
    {}

    void reset()
    {
        valid = false;
        start = -1;
        end = -1;
        nodes.clear();
    }
};


// --------------------------------------------------------------------------------------------------------------------
// PathFinder
// --------------------------------------------------------------------------------------------------------------------

// Finds paths between pairs of probes (at run-time) or from one probe to all other probes (when baking), using
// information in a visibility graph.
class PathFinder
{
public:
    struct PriorityQueueEntry
    {
        int nodeIndex;
        float cost;
    };

    // Initializes a PathFinder.
    PathFinder(const ProbeBatch& probes,
               int numThreads);

    // Finds shortest paths from the start probe to every other probe. Intended for use when baking paths as a
    // preprocess.
    void findAllShortestPaths(const IScene& scene,
                              const ProbeBatch& probes,
                              const ProbeVisibilityGraph& visGraph,
                              int start,
                              float radius,
                              float threshold,
                              float pathRange,
                              int threadIndex,
                              ProbePath* paths) const;

    // Finds the shortest path from the start probe to the end probe. Intended for use when recalculating paths
    // on the fly.
    ProbePath findShortestPath(const IScene& scene,
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
                               int threadIndex = 0) const;

private:
    Array<int, 2> mParents; // Per-thread array indicating the predecessor of each node, used during path finding.
    Array<float, 2> mCosts; // Per-thread array indicating the cost of each node, used during path finding.
    mutable vector<priority_queue<PriorityQueueEntry>> mPriorityQueue; // Per-thread priority queues for use during path finding.

    // Simplifies paths computed by findShortestPath. Typically, the visGraph passed to findShortestPath will have a
    // shorter visibility range than what was used for baking, for perf reasons. This can cause paths to be jagged.
    // This process simplifies them by skipping nodes when possible: in the probe sequence i, i+1, i+2, if i can see
    // i+2, then i+1 is removed from the path and an edge is added between i and i+2.
    void simplifyPath(const IScene& scene,
                      const ProbeBatch& probes,
                      const ProbeVisibilityGraph& visGraph,
                      const ProbeVisibilityTester& visTester,
                      int start,
                      int end,
                      float radius,
                      float threshold,
                      bool realTimeVis,
                      int* parents) const;
};

bool operator<(const PathFinder::PriorityQueueEntry& lhs,
               const PathFinder::PriorityQueueEntry& rhs);

}
