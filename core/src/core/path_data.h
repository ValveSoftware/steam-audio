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

#include "thread_pool.h"
#include "path_finder.h"
#include "probe_data.h"

#include "path_data.fbs.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SoundPath
// --------------------------------------------------------------------------------------------------------------------

// The minimal metadata required to fully describe a sound path, and to convert it to a virtual source.
class SoundPath
{
public:
    int16_t firstProbe; // The second probe in the sequence of probes from start to end.
    int16_t lastProbe; // The second-to-last probe in the sequence of probes from start to end.
    int16_t probeAfterFirst; // Valid if >= 2 probes.
    int16_t probeBeforeLast; // Valid if >= 2 probes.
    bool direct; // Is this a direct path?
    float distanceInternal; // Total distance along the path from firstProbe to lastProbe.
    float deviationInternal; // Total deviation angle along the path from firstProbe to lastProbe.

    // Initializes an invalid sound path.
    SoundPath()
        : firstProbe(-1)
        , lastProbe(-1)
        , probeAfterFirst(-1)
        , probeBeforeLast(-1)
        , direct(false)
        , distanceInternal(0.0f)
        , deviationInternal(0.0f)
    {}

    // Initializes a sound path from a probe path.
    SoundPath(const ProbePath& probePath,
              const ProbeBatch& probes);

    // Is this path valid?
    bool isValid() const
    {
        return (direct || (firstProbe >= 0 && lastProbe >= 0));
    }

    // Total distance along the path from the start probe to the end probe.
    float distance(const ProbeBatch& probes,
                   int start,
                   int end) const;

    // Total distance along the path from the source position to the end probe.
    float distance(const ProbeBatch& probes,
                   const Vector3f& source,
                   int end) const;

    // Total deviation along the path from the start probe to the end probe.
    float deviation(const ProbeBatch& probes,
                    int start,
                    int end) const;

    // Given an end probe, returns a point that corresponds to a virtual source position: the distance from the
    // virtual source to the end probe is the total path length, and the direction is the direction from the
    // lastProbe to the end probe.
    Vector3f toVirtualSource(const ProbeBatch& probes,
                             int start,
                             int end) const;

    // Given an end probe, returns a point that corresponds to a virtual source position: the distance from the
    // virtual source to the end probe is the total path length, and the direction is the direction from the
    // lastProbe to the end probe.
    Vector3f toVirtualSource(const ProbeBatch& probes,
                             const Vector3f& source,
                             int end) const;
};


// --------------------------------------------------------------------------------------------------------------------
// SoundPathRef
// --------------------------------------------------------------------------------------------------------------------

// A reference to a SoundPath. For efficiency, we retain only the unique SoundPaths, and use SoundPathRefs to index
// into the SoundPath array.
class SoundPathRef
{
public:
    int32_t index; // Index of the SoundPath. 0 refers to an invalid SoundPath.

    // Initializes this object to refer to an invalid SoundPath.
    SoundPathRef()
        : index(0)
    {}
};


// --------------------------------------------------------------------------------------------------------------------
// BakedPathData
// --------------------------------------------------------------------------------------------------------------------

// Represents the baked data used for looking up paths at runtime. This is the data that should be serialized to disk
// during baking. The data stored is a SoundPath for every pair of probes.
class BakedPathData : public IBakedData
{
public:
    // Generates baked data given an array of probes. This involves first creating a visibility graph, then calculating
    // shortest paths between every pair of probes. If "debug mode" is specified, then in addition to SoundPaths,
    // we store a ProbePath for every pair of probes, to allow visualization.
    BakedPathData(const IScene& scene,
                  const ProbeBatch& probes,
                  int numSamples,
                  float radius,
                  float threshold,
                  float visRange,
                  float visRangeRealTime,
                  float pathRange,
                  bool asymmetricVisRange,
                  const Vector3f& down,
                  bool pruneVisGraph,
                  int numThreads,
                  ThreadPool& threadPool,
                  std::atomic<bool>& cancel,
                  ProgressCallback progressCallback = nullptr,
                  void* callbackUserData = nullptr);

    // Loads baked data from a serialized object.
    BakedPathData(const Serialized::BakedPathingData* serializedObject);

    virtual void updateProbePosition(int index,
                                     const Vector3f& position) override
    {
        mNeedsUpdate = true;
    }

    virtual void addProbe(const Sphere& influence) override
    {
        mNeedsUpdate = true;
    }

    virtual void removeProbe(int index) override
    {
        mNeedsUpdate = true;
    }

    virtual void updateEndpoint(const BakedDataIdentifier& identifier,
                                const Probe* probes,
                                const Sphere& endpointInfluence) override
    {}

    // Returns the visibility graph.
    ProbeVisibilityGraph const& visGraph() const
    {
        return *mVisGraph;
    }

    bool needsUpdate() const
    {
        return mNeedsUpdate;
    }

    // Queries the baked data for the shortest path between the start probe and the end probe.
    SoundPath lookupShortestPath(int start,
                                 int end,
                                 ProbePath* probePath) const;

    // Returns the size (in bytes) of the baked data.
    virtual uint64_t serializedSize() const override;

    // Saves the baked data to a serialized object.
    flatbuffers::Offset<Serialized::BakedPathingData> serialize(SerializedObject& serializedObject) const;

private:
    unique_ptr<ProbeVisibilityGraph> mVisGraph; // The visibility graph.
    Array<SoundPath> mUniqueBakedPaths; // The unique SoundPaths.
    Array<SoundPathRef, 2> mBakedPathRefs; // SoundPathRefs for SoundPaths between every pair of probes.
    bool mNeedsUpdate;

    void reconstructProbePath(int start,
                              int end,
                              const SoundPath& soundPath,
                              ProbePath& probePath) const;
};


// --------------------------------------------------------------------------------------------------------------------
// PathBaker
// --------------------------------------------------------------------------------------------------------------------

class PathBaker
{
public:
    static void bake(const IScene& scene,
                     const BakedDataIdentifier& identifier,
                     int numSamples,
                     float radius,
                     float threshold,
                     float visRange,
                     float visRangeRealTime,
                     float pathRange,
                     bool asymmetricVisRange,
                     const Vector3f& down,
                     bool pruneVisGraph,
                     int numThreads,
                     ProbeBatch& probes,
                     ProgressCallback progressCallback = nullptr,
                     void* callbackUserData = nullptr);

    static void cancel();

private:
    static std::atomic<bool> sBakeInProgress;
    static ThreadPool* sThreadPool;
    static std::atomic<bool> sCancel;
};

}
