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

#include "probe_manager.h"
#include "profiler.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// ProbeNeighborhood
// ---------------------------------------------------------------------------------------------------------------------

void ProbeNeighborhood::resize(int maxProbes)
{
    batches.resize(maxProbes);
    probeIndices.resize(maxProbes);
    weights.resize(maxProbes);

    rays.resize(maxProbes);
    minDistances.resize(maxProbes);
    maxDistances.resize(maxProbes);
    rayMapping.resize(maxProbes);
    isOccluded.resize(maxProbes);

    reset();
}

void ProbeNeighborhood::reset()
{
    for (auto i = 0u; i < batches.size(0); ++i)
    {
        batches[i] = nullptr;
        probeIndices[i] = -1;
        weights[i] = 0.0f;
    }
}

void ProbeNeighborhood::checkOcclusion(const IScene& scene,
                                       const Vector3f& point)
{
    PROFILE_FUNCTION();

    int nProbes = numProbes();
    int nValidProbes = numValidProbes();

    for (auto i = 0, j = 0; i < nProbes; ++i)
    {
        if (batches[i] && probeIndices[i] >= 0)
        {
            Vector3f dir = (*batches[i])[probeIndices[i]].influence.center - point;
            rays[j] = { point, Vector3f::unitVector(dir) };
            minDistances[j] = 0.0f;
            maxDistances[j] = dir.length();
            rayMapping[j] = i;
            ++j;
        }
    }

    scene.anyHits(nValidProbes, rays.data(), minDistances.data(), maxDistances.data(), isOccluded.data());

    for (auto j = 0; j < nValidProbes; ++j)
    {
        if (isOccluded[j])
        {
            batches[rayMapping[j]] = nullptr;
            probeIndices[rayMapping[j]] = -1;
        }
    }
}

int ProbeNeighborhood::findNearest(const Vector3f& point) const
{
    auto minDistance = std::numeric_limits<float>::infinity();
    auto minIndex = 0;

    for (auto i = 0u; i < weights.size(0); ++i)
    {
        if (batches[i] && probeIndices[i] >= 0)
        {
            const auto& probePosition = (*batches[i])[probeIndices[i]].influence.center;

            auto distance = (point - probePosition).lengthSquared();
            if (distance < minDistance)
            {
                minDistance = distance;
                minIndex = i;
            }
        }
    }

    return minIndex;
}

void ProbeNeighborhood::getProbe(int neighborProbeIndex, int* probeIndex, float* weight)
{
    if (neighborProbeIndex < 0 || neighborProbeIndex >= weights.size(0))
        return;

    if (probeIndex)
        *probeIndex = probeIndices[neighborProbeIndex];

    if (weight)
        *weight = weights[neighborProbeIndex];
}

void ProbeNeighborhood::calcWeights(const Vector3f& point)
{
    PROFILE_FUNCTION();

    float totalWeight = 0.0f;
    for (auto i = 0u; i < weights.size(0); ++i)
    {
        if (batches[i] && probeIndices[i] >= 0)
        {
            // Offset zero distance. Evaluate exponential weights in future.
            weights[i] = 1.0f / ((point - (*batches[i])[probeIndices[i]].influence.center).length() + 1e-4f);
            totalWeight += weights[i];
        }
    }

    for (auto i = 0u; i < weights.size(0); ++i)
    {
        if (batches[i] && probeIndices[i] >= 0)
        {
            weights[i] /= totalWeight;
        }
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// ProbeManager
// ---------------------------------------------------------------------------------------------------------------------

void ProbeManager::addProbeBatch(shared_ptr<ProbeBatch> probeBatch)
{
    mProbeBatches[1].push_back(probeBatch);
}

void ProbeManager::removeProbeBatch(shared_ptr<ProbeBatch> probeBatch)
{
    mProbeBatches[1].remove(probeBatch);
}

void ProbeManager::commit()
{
    mProbeBatches[0] = mProbeBatches[1];
}

void ProbeManager::getInfluencingProbes(const Vector3f& point,
                                        ProbeNeighborhood& neighborhood)
{
    PROFILE_FUNCTION();

    auto numProbes = static_cast<int>(mProbeBatches[0].size() * ProbeNeighborhood::kMaxProbesPerBatch);
    if (neighborhood.numProbes() != numProbes)
    {
        neighborhood.resize(numProbes);
    }
    else
    {
        neighborhood.reset();
    }

    auto offset = 0;
    for (const auto& batch : mProbeBatches[0])
    {
        batch->getInfluencingProbes(point, neighborhood, offset);
        offset += ProbeNeighborhood::kMaxProbesPerBatch;
    }
}

}
