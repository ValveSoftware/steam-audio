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

#include "path_data.h"

#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SoundPath
// --------------------------------------------------------------------------------------------------------------------

SoundPath::SoundPath(const ProbePath& probePath,
                     const ProbeBatch& probes)
    : SoundPath()
{
    if (probePath.valid)
    {
        if (probePath.nodes.empty())
        {
            direct = true;
        }
        else
        {
            firstProbe = probePath.nodes.front();
            lastProbe = probePath.nodes.back();

            if (probePath.nodes.size() >= 2)
            {
                probeAfterFirst = probePath.nodes[1];
                probeBeforeLast = probePath.nodes[probePath.nodes.size() - 2];
            }

            for (auto i = 1u; i < probePath.nodes.size(); ++i)
            {
                const auto& prevPoint = probes[probePath.nodes[i - 1]].influence.center;
                const auto& curPoint = probes[probePath.nodes[i]].influence.center;

                distanceInternal += (curPoint - prevPoint).length();
            }

            for (auto i = 1u; i < probePath.nodes.size() - 1; ++i)
            {
                const auto& prevPoint = probes[probePath.nodes[i - 1]].influence.center;
                const auto& curPoint = probes[probePath.nodes[i]].influence.center;
                const auto& nextPoint = probes[probePath.nodes[i + 1]].influence.center;

                auto prevDir = Vector3f::unitVector(curPoint - prevPoint);
                auto nextDir = Vector3f::unitVector(nextPoint - curPoint);

                deviationInternal += Vector3f::angleBetween(prevDir, nextDir);
            }
        }
    }
}

float SoundPath::distance(const ProbeBatch& probes,
                          int start,
                          int end) const
{
    assert(isValid());

    if (direct)
    {
        return (probes[end].influence.center - probes[start].influence.center).length();
    }
    else
    {
        auto result = distanceInternal;
        result += (probes[firstProbe].influence.center - probes[start].influence.center).length();
        result += (probes[end].influence.center - probes[lastProbe].influence.center).length();
        return result;
    }
}

float SoundPath::distance(const ProbeBatch& probes,
                          const Vector3f& source,
                          int end) const
{
    assert(isValid());

    auto result = distanceInternal;

    if (!direct)
    {
        result += (probes[firstProbe].influence.center - source).length();
        result += (probes[end].influence.center - probes[lastProbe].influence.center).length();
    }

    return result;
}

float SoundPath::deviation(const ProbeBatch& probes,
                           int start,
                           int end) const
{
    assert(isValid());

    auto result = deviationInternal;

    if (!direct)
    {
        if (probeAfterFirst < 0 && probeBeforeLast < 0)
        {
            auto const& prevPoint = probes[start].influence.center;
            auto const& curPoint = probes[firstProbe].influence.center;
            auto const& nextPoint = probes[end].influence.center;

            auto prevDir = Vector3f::unitVector(curPoint - prevPoint);
            auto nextDir = Vector3f::unitVector(nextPoint - curPoint);

            result += Vector3f::angleBetween(prevDir, nextDir);
        }
        else
        {
            if (probeAfterFirst >= 0)
            {
                auto const& prevPoint = probes[start].influence.center;
                auto const& curPoint = probes[firstProbe].influence.center;
                auto const& nextPoint = probes[probeAfterFirst].influence.center;

                auto prevDir = Vector3f::unitVector(curPoint - prevPoint);
                auto nextDir = Vector3f::unitVector(nextPoint - curPoint);

                result += Vector3f::angleBetween(prevDir, nextDir);
            }

            if (probeBeforeLast >= 0)
            {
                auto const& prevPoint = probes[probeBeforeLast].influence.center;
                auto const& curPoint = probes[lastProbe].influence.center;
                auto const& nextPoint = probes[end].influence.center;

                auto prevDir = Vector3f::unitVector(curPoint - prevPoint);
                auto nextDir = Vector3f::unitVector(nextPoint - curPoint);

                result += Vector3f::angleBetween(prevDir, nextDir);
            }
        }
    }

    return result;
}

Vector3f SoundPath::toVirtualSource(const ProbeBatch& probes,
                                    int start,
                                    int end) const
{
    if (direct)
    {
        return probes[start].influence.center;
    }
    else
    {
        auto totalDistance = distance(probes, start, end);
        auto direction = Vector3f::unitVector(probes[lastProbe].influence.center - probes[end].influence.center);
        return probes[end].influence.center + (totalDistance * direction);
    }
}

Vector3f SoundPath::toVirtualSource(const ProbeBatch& probes,
                                    const Vector3f& source,
                                    int end) const
{
    if (direct)
    {
        return source;
    }
    else
    {
        auto totalDistance = distance(probes, source, end);
        auto direction = Vector3f::unitVector(probes[lastProbe].influence.center - probes[end].influence.center);
        return probes[end].influence.center + (totalDistance * direction);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// BakedPathData
// --------------------------------------------------------------------------------------------------------------------

BakedPathData::BakedPathData(const IScene& scene,
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
                             ProgressCallback progressCallback,
                             void* callbackUserData)
    : mBakedPathRefs(probes.numProbes(), probes.numProbes())
{
    // First, generate the visibility graph.
    ProbeVisibilityTester visTester(numSamples, asymmetricVisRange, down);
    mVisGraph = ipl::make_unique<ProbeVisibilityGraph>(scene, probes, visTester, radius, threshold, visRange,
                                                       numThreads, cancel, progressCallback, callbackUserData);

    // Next, using multiple threads, calculate shortest paths between every pair of probes.
    PathFinder pathFinder(probes, numThreads);
    Array<ProbePath, 2> probePaths(probes.numProbes(), probes.numProbes());
    Array<ProbePath, 2> threadPaths(numThreads, probes.numProbes());
    auto totalIterations = probes.numProbes() * probes.numProbes();
    std::atomic<int> iterationsDone(0);
    JobGraph jobGraph;

    if (cancel)
    {
        cancel = false;
        return;
    }

    // Allow a certain number of maximum probes to be baked in parallel so that progress callback
    // can be called from the main thread.
    const int kMaxProbesToBakeInParallel = 50;
    for (auto i = 0; i < probes.numProbes();)
    {
        if (cancel)
        {
            cancel = false;
            return;
        }

        for (auto k = 0; k < kMaxProbesToBakeInParallel && i < probes.numProbes(); ++i, ++k)
        {
            auto bakeJob = [this, i, &scene, &probes, radius, threshold, pathRange, progressCallback,
                callbackUserData, &pathFinder, &probePaths, &threadPaths, &totalIterations, &iterationsDone]
                (int threadIndex,
                    std::atomic<bool>&)
            {
                PROFILE_ZONE("BakedPathData::bakeJob");
                for (auto j = 0; j < probes.numProbes(); ++j)
                {
                    threadPaths[threadIndex][j].nodes.clear();
                }

                pathFinder.findAllShortestPaths(scene, probes, *mVisGraph, i, radius, threshold, pathRange,
                    threadIndex, threadPaths[threadIndex]);

                for (auto j = 0; j < probes.numProbes(); ++j)
                {
                    probePaths[i][j] = threadPaths[threadIndex][j];
                    ++iterationsDone;
                }
            };

            jobGraph.addJob(bakeJob);
        }

        threadPool.process(jobGraph);

        if (progressCallback)
        {
            progressCallback(static_cast<float>(i) / probes.numProbes(), callbackUserData);
        }
    }

    // Remove all data with j > i, since they can be reconstructed from the data with j < i due to symmetry.
    ProbePath invalidProbePath;
    for (auto i = 0; i < probes.numProbes(); ++i)
    {
        for (auto j = i + 1; j < probes.numProbes(); ++j)
        {
            probePaths[i][j] = invalidProbePath;
        }
    }

    if (cancel)
    {
        cancel = false;
        return;
    }

    // Sort the probe paths.
    auto compareProbePaths = [](const ProbePath& lhs,
                                const ProbePath& rhs)
    {
        if (!lhs.valid && rhs.valid)
            return true;

        return std::lexicographical_compare(lhs.nodes.begin(), lhs.nodes.end(), rhs.nodes.begin(), rhs.nodes.end());
    };

    std::sort(probePaths.flatData(), probePaths.flatData() + probePaths.totalSize(), compareProbePaths);

    if (cancel)
    {
        cancel = false;
        return;
    }

    // Extract all the unique sound paths. At the end of this process, mUniqueBakedPaths contains the k unique
    // sound paths, and mBakedPathRefs are n^2 indices (each between 0 and k-1), into the mUniqueBakedPaths
    // array.
    SoundPathRef invalidSoundPathRef;
    for (auto i = 0; i < probes.numProbes(); ++i)
    {
        for (auto j = 0; j < probes.numProbes(); ++j)
        {
            mBakedPathRefs[i][j] = invalidSoundPathRef;
        }
    }

    auto areProbePathsEqual = [](const ProbePath& lhs,
                                 const ProbePath& rhs)
    {
        if (lhs.valid ^ rhs.valid)
            return false;

        if (lhs.nodes.size() != rhs.nodes.size())
            return false;

        for (auto i = 0u; i < lhs.nodes.size(); ++i)
        {
            if (lhs.nodes[i] != rhs.nodes[i])
                return false;
        }

        return true;
    };

    if (cancel)
    {
        cancel = false;
        return;
    }

    vector<SoundPath> uniqueSoundPaths;
    for (auto i = 0, index = 0; i < probes.numProbes(); ++i)
    {
        for (auto j = 0; j < probes.numProbes(); ++j, ++index)
        {
            if (index == 0 || !areProbePathsEqual(probePaths.flatData()[index], probePaths.flatData()[index - 1]))
            {
                uniqueSoundPaths.push_back(SoundPath(probePaths.flatData()[index], probes));
            }

            if (uniqueSoundPaths.back().isValid())
            {
                auto start = probePaths.flatData()[index].start;
                auto end = probePaths.flatData()[index].end;

                mBakedPathRefs[start][end].index = static_cast<int>(uniqueSoundPaths.size()) - 1;
            }
        }

        if (cancel)
        {
            cancel = false;
            return;
        }

        if (progressCallback)
        {
            progressCallback(static_cast<float>(index) / probePaths.totalSize(), callbackUserData);
        }
    }

    mUniqueBakedPaths.resize(uniqueSoundPaths.size());
    memcpy(mUniqueBakedPaths.data(), uniqueSoundPaths.data(), uniqueSoundPaths.size() * sizeof(SoundPath));
    if (progressCallback)
    {
        progressCallback(1.0f, callbackUserData);
    }

    if (pruneVisGraph)
    {
        mVisGraph->prune(probes, visTester, visRangeRealTime);
    }

    if (cancel)
    {
        cancel = false;
        return;
    }
}

BakedPathData::BakedPathData(const Serialized::BakedPathingData* serializedObject)
{
    assert(serializedObject);
    assert(serializedObject->vis_graph() && serializedObject->vis_graph()->nodes() && serializedObject->vis_graph()->nodes()->Length() > 0);
    assert(serializedObject->unique_paths() && serializedObject->unique_paths()->Length() > 0);
    assert(serializedObject->path_indices() && serializedObject->path_indices()->Length() > 0);
    assert(serializedObject->paths() && serializedObject->paths()->Length() > 0);

    // # probes
    auto numProbes = serializedObject->vis_graph()->nodes()->Length();

    // vis graph
    mVisGraph = ipl::make_unique<ProbeVisibilityGraph>(serializedObject->vis_graph());

    // # valid SoundPaths
    auto numValidPaths = serializedObject->paths()->Length();

    // # unique SoundPaths
    auto numUniquePaths = serializedObject->unique_paths()->Length();
    mUniqueBakedPaths.resize(numUniquePaths);

    // unique SoundPaths
    for (auto i = 0u; i < numUniquePaths; ++i)
    {
        mUniqueBakedPaths[i].firstProbe = serializedObject->unique_paths()->Get(i)->first_probe();
        mUniqueBakedPaths[i].lastProbe = serializedObject->unique_paths()->Get(i)->last_probe();
        mUniqueBakedPaths[i].probeAfterFirst = serializedObject->unique_paths()->Get(i)->probe_after_first();
        mUniqueBakedPaths[i].probeBeforeLast = serializedObject->unique_paths()->Get(i)->probe_before_last();
        mUniqueBakedPaths[i].direct = serializedObject->unique_paths()->Get(i)->direct();
        mUniqueBakedPaths[i].distanceInternal = serializedObject->unique_paths()->Get(i)->distance_internal();
        mUniqueBakedPaths[i].deviationInternal = serializedObject->unique_paths()->Get(i)->deviation_internal();
    }

    // SoundPathRefs (index/value pair for each valid path)
    mBakedPathRefs.resize(numProbes, numProbes);

    SoundPathRef invalidSoundPathRef;
    for (auto i = 0u; i < numProbes; ++i)
    {
        for (auto j = 0u; j < numProbes; ++j)
        {
            mBakedPathRefs[i][j] = invalidSoundPathRef;
        }
    }

    for (auto i = 0u; i < numValidPaths; ++i)
    {
        auto index = serializedObject->path_indices()->Get(i);
        mBakedPathRefs.flatData()[index].index = serializedObject->paths()->Get(i);
    }
}

SoundPath BakedPathData::lookupShortestPath(int start,
                                            int end,
                                            ProbePath* probePath) const
{
    PROFILE_FUNCTION();

    SoundPath soundPath;

    if (start < end)
    {
        soundPath = mUniqueBakedPaths[mBakedPathRefs[end][start].index];
        std::swap(soundPath.firstProbe, soundPath.lastProbe);
        std::swap(soundPath.probeAfterFirst, soundPath.probeBeforeLast);
    }
    else
    {
        soundPath = mUniqueBakedPaths[mBakedPathRefs[start][end].index];
    }

    if (probePath)
    {
        reconstructProbePath(start, end, soundPath, *probePath);
    }

    return soundPath;
}

void BakedPathData::reconstructProbePath(int start,
                                         int end,
                                         const SoundPath& soundPath,
                                         ProbePath& probePath) const
{
    PROFILE_FUNCTION();

    probePath.valid = (soundPath.isValid());

    if (probePath.valid)
    {
        probePath.start = start;
        probePath.end = end;

        auto current = end;
        auto prev = (soundPath.direct) ? start : soundPath.lastProbe;

        while (current != start)
        {
            if (current != start && current != end)
            {
                probePath.nodes.push_back(current);
            }

            if (prev == start)
                break;

            auto nextPath = lookupShortestPath(start, prev, nullptr);
            if (!nextPath.isValid())
            {
                probePath.reset();
                return;
            }

            current = prev;
            prev = (nextPath.direct) ? start : nextPath.lastProbe;
        }

        std::reverse(probePath.nodes.begin(), probePath.nodes.end());
    }
}

uint64_t BakedPathData::serializedSize() const
{
    // # probes
    uint64_t size = sizeof(int32_t);

    // vis graph
    size += mVisGraph->serializedSize();

    // # valid SoundPaths
    size += sizeof(int32_t);

    // # unique SoundPaths
    size += sizeof(int32_t);

    // unique SoundPaths
    size += mUniqueBakedPaths.totalSize() * sizeof(SoundPath);

    // SoundPathRefs. For valid paths only.
    auto numProbes = mBakedPathRefs.size(0);
    for (auto i = 0u; i < numProbes; ++i)
    {
        for (auto j = 0u; j < numProbes; ++j)
        {
            if (mUniqueBakedPaths[mBakedPathRefs[i][j].index].isValid())
            {
                size += sizeof(int32_t) + sizeof(SoundPathRef);
            }
        }
    }

    return size;
}

flatbuffers::Offset<Serialized::BakedPathingData> BakedPathData::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    auto visGraphOffset = mVisGraph->serialize(serializedObject);

    vector<flatbuffers::Offset<Serialized::SoundPath>> soundPathOffsets(mUniqueBakedPaths.totalSize());
    for (auto i = 0u; i < mUniqueBakedPaths.totalSize(); ++i)
    {
        soundPathOffsets[i] = Serialized::CreateSoundPath(fbb, mUniqueBakedPaths[i].firstProbe,
                                                          mUniqueBakedPaths[i].lastProbe,
                                                          mUniqueBakedPaths[i].probeAfterFirst,
                                                          mUniqueBakedPaths[i].probeBeforeLast,
                                                          mUniqueBakedPaths[i].direct,
                                                          mUniqueBakedPaths[i].distanceInternal,
                                                          mUniqueBakedPaths[i].deviationInternal);
    }

    auto soundPathsOffset = fbb.CreateVector(soundPathOffsets.data(), soundPathOffsets.size());

    auto numProbes = mBakedPathRefs.size(0);
    auto numValidPaths = 0;
    for (auto i = 0u; i < numProbes; ++i)
    {
        for (auto j = 0u; j < numProbes; ++j)
        {
            if (mUniqueBakedPaths[mBakedPathRefs[i][j].index].isValid())
            {
                ++numValidPaths;
            }
        }
    }

    vector<int32_t> pathIndices;
    vector<int32_t> paths;
    pathIndices.reserve(numValidPaths);
    paths.reserve(numValidPaths);
    for (auto i = 0u; i < numProbes; ++i)
    {
        for (auto j = 0u; j < numProbes; ++j)
        {
            if (mUniqueBakedPaths[mBakedPathRefs[i][j].index].isValid())
            {
                pathIndices.push_back(static_cast<int>(i * numProbes + j));
                paths.push_back(mBakedPathRefs[i][j].index);
            }
        }
    }

    auto pathIndicesOffset = fbb.CreateVector(pathIndices.data(), pathIndices.size());
    auto pathsOffset = fbb.CreateVector(paths.data(), paths.size());

    return Serialized::CreateBakedPathingData(fbb, visGraphOffset, soundPathsOffset, pathIndicesOffset, pathsOffset);
}


// --------------------------------------------------------------------------------------------------------------------
// PathBaker
// --------------------------------------------------------------------------------------------------------------------

std::atomic<bool> PathBaker::sCancel(false);
std::atomic<bool> PathBaker::sBakeInProgress(false);
ThreadPool* PathBaker::sThreadPool = nullptr;

void PathBaker::bake(const IScene& scene,
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
                     ProgressCallback progressCallback,
                     void* callbackUserData)
{
    PROFILE_FUNCTION();

    assert(identifier.type == BakedDataType::Pathing);
    assert(identifier.variation == BakedDataVariation::Dynamic);

    sBakeInProgress = true;
    sCancel = false;

    ThreadPool threadPool(numThreads);
    sThreadPool = &threadPool;

    if (probes.hasData(identifier))
    {
        probes.removeData(identifier);
    }

    probes.addData(identifier, ipl::make_unique<BakedPathData>(scene, probes, numSamples, radius, threshold, visRange,
                                                          visRangeRealTime, pathRange, asymmetricVisRange, down,
                                                          pruneVisGraph, numThreads, threadPool, sCancel, progressCallback,
                                                          callbackUserData));

    sThreadPool = nullptr;
    sBakeInProgress = false;
}

void PathBaker::cancel()
{
    if (sBakeInProgress && sThreadPool)
    {
        sCancel = true;
        sThreadPool->cancel();
    }
}

}
