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

#include "path_simulator.h"

#include "eq_effect.h"
#include "math_functions.h"
#include "sampling.h"
#include "sh.h"
#include "profiler.h"
#include "propagation_medium.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// PathSimulator
// --------------------------------------------------------------------------------------------------------------------

bool PathSimulator::sEnablePathsFromAllSourceProbes = false;

PathSimulator::PathSimulator(const ProbeBatch& probes,
                             int numSamples,
                             bool asymmetricVisRange,
                             const Vector3f& down)
    : mVisTester(numSamples, asymmetricVisRange, down)
    , mPathFinder(probes, 1)
{}

bool PathSimulator::isPathOccluded(const SoundPath& path,
                                   const IScene& scene,
                                   const ProbeBatch& probes,
                                   float radius,
                                   float threshold,
                                   int start,
                                   int end,
                                   bool enableValidation,
                                   ValidationRayVisualizationCallback validationRayVisualization,
                                   void* userData) const
{
    PROFILE_FUNCTION();

    if (enableValidation || validationRayVisualization)
    {
        BakedDataIdentifier identifier;
        identifier.variation = BakedDataVariation::Dynamic;
        identifier.type = BakedDataType::Pathing;

        const auto& bakedPathData = static_cast<const BakedPathData&>(probes[identifier]);

        auto current = end;
        auto prev = (path.direct) ? start : path.lastProbe;

        while (current != start)
        {
            bool probeVisible = enableValidation ? mVisTester.areProbesVisible(scene, probes, current, prev, radius, threshold) : true;
            if (!probeVisible)
            {
                if (validationRayVisualization)
                {
                    validationRayVisualization(probes[prev].influence.center,
                                               probes[current].influence.center, true, userData);
                }

                return true;
            }
            else
            {
                if (validationRayVisualization)
                {
                    validationRayVisualization(probes[prev].influence.center,
                                               probes[current].influence.center, false, userData);
                }
            }

            if (prev == start)
                break;

            auto nextPath = bakedPathData.lookupShortestPath(start, prev, nullptr);
            if (!nextPath.isValid())
            {
                return true;
            }

            current = prev;
            prev = (nextPath.direct) ? start : nextPath.lastProbe;
        }

        return false;
    }
    else
    {
        return false;
    }
}

// First, find the source-probe (the probe nearest to the source), and the listener-probes (all probes which influence
// the listener). For each listener-probe, query the baked data for the shortest path from the source-probe to the
// listener-probe.
//
// Weights are calculated for the paths reaching each listener-probe, such that if the listener is closer to a given
// listener-probe, its corresponding weight is larger.
//
// Optionally, we validate paths, by testing rays between every consecutive pair of probes.
//
// Optionally, if a baked path is found to be invalid (typically due to the presence of dynamic occluders), we search
// for alternate paths.
//
// Optionally, if the source and listener are in line of sight (this visibility check is a single ray cast), we create
// a SoundPath describing this.
//
// Finally, all the paths that haven't been discarded are weighted and summed into a set of SH and EQ coefficients.
bool PathSimulator::findPaths(const Vector3f& source,
                              const Vector3f& listener,
                              const IScene& scene,
                              const ProbeBatch& probes,
                              const ProbeNeighborhood& sourceProbes,
                              const ProbeNeighborhood& listenerProbes,
                              float radius,
                              float threshold,
                              float visRange,
                              int order,
                              bool enableValidation,
                              bool findAlternatePaths,
                              bool simplifyPaths,
                              bool realTimeVis,
                              float* eqGains,
                              float* coeffs,
                              Vector3f* avgDirection,
                              float* distanceRatio,
                              ValidationRayVisualizationCallback validationRayVisualization,
                              void* userData,
                              bool forceDirectOcclusion)
{
    PROFILE_FUNCTION();

    const int kMaxPaths = 64;

    int numPaths = 0;
    SoundPath paths[kMaxPaths];
    float pathWeights[kMaxPaths];
    int starts[kMaxPaths];
    int ends[kMaxPaths];

    if (scene.isOccluded(listener, source) || forceDirectOcclusion)
    {
        if (sourceProbes.hasValidProbes() && listenerProbes.hasValidProbes())
        {
            BakedDataIdentifier identifier;
            identifier.variation = BakedDataVariation::Dynamic;
            identifier.type = BakedDataType::Pathing;

            if (probes.hasData(identifier))
            {
                const auto& bakedPathData = static_cast<const BakedPathData&>(probes[identifier]);

                if (sEnablePathsFromAllSourceProbes)
                {
                    for (auto i = 0; i < sourceProbes.numProbes(); ++i)
                    {
                        findPathsFromSourceProbe(scene, probes, sourceProbes, listenerProbes, bakedPathData, i, sourceProbes.weights[i],
                                                 radius, threshold, visRange, enableValidation, findAlternatePaths, simplifyPaths, realTimeVis,
                                                 validationRayVisualization, userData, numPaths, paths, pathWeights, starts, ends);
                    }
                }
                else
                {
                    findPathsFromSourceProbe(scene, probes, sourceProbes, listenerProbes, bakedPathData, sourceProbes.findNearest(source), 1.0f,
                                             radius, threshold, visRange, enableValidation, findAlternatePaths, simplifyPaths, realTimeVis,
                                             validationRayVisualization, userData, numPaths, paths, pathWeights, starts, ends);
                }
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        SoundPath directPath;
        directPath.direct = true;

        paths[numPaths] = directPath;
        pathWeights[numPaths] = 1.0f;
        starts[numPaths] = -1;
        ends[numPaths] = -1;
        ++numPaths;
    }

    calcAmbisonicsCoeffsForPaths(source, listener, probes, starts, ends, numPaths, paths, pathWeights, order,
                                 eqGains, coeffs);

    calcAverageDirectionForPaths(source, listener, probes, starts, ends, numPaths, paths, pathWeights, avgDirection);

    calcDistanceRatioForPaths(source, probes, starts, ends, numPaths, paths, pathWeights, distanceRatio);

    return true;
}

void PathSimulator::findPathsFromSourceProbe(const IScene& scene,
                                             const ProbeBatch& probes,
                                             const ProbeNeighborhood& sourceProbes,
                                             const ProbeNeighborhood& listenerProbes,
                                             const BakedPathData& bakedPathData,
                                             int sourceProbeNeighborhoodIndex,
                                             float sourceProbeWeight,
                                             float radius,
                                             float threshold,
                                             float visRange,
                                             bool enableValidation,
                                             bool findAlternatePaths,
                                             bool simplifyPaths,
                                             bool realTimeVis,
                                             ValidationRayVisualizationCallback validationRayVisualization,
                                             void* userData,
                                             int& numPaths,
                                             SoundPath* paths,
                                             float* pathWeights,
                                             int* starts,
                                             int* ends)
{
    if (!sourceProbes.batches[sourceProbeNeighborhoodIndex] || sourceProbes.probeIndices[sourceProbeNeighborhoodIndex] < 0 || sourceProbes.batches[sourceProbeNeighborhoodIndex] != &probes)
        return;

    auto sourceProbeIndex = sourceProbes.probeIndices[sourceProbeNeighborhoodIndex];

    for (auto i = 0; i < listenerProbes.numProbes(); ++i)
    {
        findPathsFromSourceProbeToListenerProbe(scene, probes, listenerProbes, bakedPathData, sourceProbeIndex, sourceProbeWeight, i,
                                                radius, threshold, visRange, enableValidation, findAlternatePaths,
                                                simplifyPaths, realTimeVis, validationRayVisualization, userData,
                                                numPaths, paths, pathWeights, starts, ends);
    }
}

void PathSimulator::findPathsFromSourceProbeToListenerProbe(const IScene& scene,
                                                            const ProbeBatch& probes,
                                                            const ProbeNeighborhood& listenerProbes,
                                                            const BakedPathData& bakedPathData,
                                                            int sourceProbeIndex,
                                                            float sourceProbeWeight,
                                                            int listenerProbeNeighborhoodIndex,
                                                            float radius,
                                                            float threshold,
                                                            float visRange,
                                                            bool enableValidation,
                                                            bool findAlternatePaths,
                                                            bool simplifyPaths,
                                                            bool realTimeVis,
                                                            ValidationRayVisualizationCallback validationRayVisualization,
                                                            void* userData,
                                                            int& numPaths,
                                                            SoundPath* paths,
                                                            float* pathWeights,
                                                            int* starts,
                                                            int* ends)
{
    if (!listenerProbes.batches[listenerProbeNeighborhoodIndex] || listenerProbes.probeIndices[listenerProbeNeighborhoodIndex] < 0 || listenerProbes.batches[listenerProbeNeighborhoodIndex] != &probes)
        return;

    auto listenerProbeIndex = listenerProbes.probeIndices[listenerProbeNeighborhoodIndex];

    SoundPath soundPath;
    auto tryRealTime = false;

    soundPath = bakedPathData.lookupShortestPath(sourceProbeIndex, listenerProbeIndex, nullptr);
    if (soundPath.isValid())
    {
        if (isPathOccluded(soundPath, scene, probes, radius, threshold, sourceProbeIndex,
            listenerProbeIndex, enableValidation, validationRayVisualization, userData))
        {
            if (findAlternatePaths)
            {
                tryRealTime = true;
            }
        }
    }

    if (tryRealTime)
    {
        ProbePath probePath;
        probePath = mPathFinder.findShortestPath(scene, probes, bakedPathData.visGraph(),
                                                 mVisTester, sourceProbeIndex, listenerProbeIndex, radius,
                                                 threshold, visRange, simplifyPaths, realTimeVis);

        soundPath = SoundPath(probePath, probes);
    }

    if (soundPath.isValid())
    {
        paths[numPaths] = soundPath;
        pathWeights[numPaths] = sourceProbeWeight * listenerProbes.weights[listenerProbeNeighborhoodIndex];
        starts[numPaths] = sourceProbeIndex;
        ends[numPaths] = listenerProbeIndex;
        ++numPaths;
    }
}

SoundPath PathSimulator::findShortestPathFromSourceProbeToListenerProbe(const IScene& scene, const ProbeBatch& probes,
    int sourceProbeIndex, int listenerProbeIndex, const BakedPathData& bakedPathData, float radius, float threshold,
    float visRange, bool enableValidation, bool findAlternatePaths, bool simplifyPaths, bool realTimeVis,
    ProbePath& probePath, ValidationRayVisualizationCallback validationRayVisualization, void* userData)
{
    if (sourceProbeIndex == listenerProbeIndex)
    {
        SoundPath soundPath{};
        soundPath.direct = true;

        return soundPath;
    }

    auto tryRealTime = false;
    auto soundPath = bakedPathData.lookupShortestPath(sourceProbeIndex, listenerProbeIndex, &probePath);
    if (soundPath.isValid())
    {
        if (isPathOccluded(soundPath, scene, probes, radius, threshold, sourceProbeIndex,
            listenerProbeIndex, enableValidation, validationRayVisualization, userData))
        {
            if (findAlternatePaths)
            {
                tryRealTime = true;
            }
        }
    }

    if (tryRealTime)
    {
        probePath = mPathFinder.findShortestPath(scene, probes, bakedPathData.visGraph(),
            mVisTester, sourceProbeIndex, listenerProbeIndex, radius,
            threshold, visRange, simplifyPaths, realTimeVis);

        soundPath = SoundPath(probePath, probes);
    }

    return soundPath;
}

// The EQ coefficients for a given total deviation angle are calculated using UTD.
// See http://www-sop.inria.fr/reves/Nicolas.Tsingos/publis/sig2001.pdf.
void PathSimulator::calcDeviationTerm(float deviation,
                                      float* deviationTerm)
{
    auto cotf = [](const float theta)
    {
        auto tan_theta = tanf(theta);
        return 1.0f / tan_theta;
    };

    auto N_plus = [](const float n,
                     const float x)
    {
        if (x <= Math::kPi * (n - 1.0f))
            return 0.0f;
        else
            return 1.0f;
    };

    auto N_minus = [](const float n,
                      const float x)
    {
        if (x < Math::kPi * (1.0f - n))
            return -1.0f;
        else if (Math::kPi * (1.0f - n) <= x && x <= Math::kPi * (1.0f + n))
            return 0.0f;
        else
            return 1.0f;
    };

    auto a = [](const float n,
                const float beta,
                const float N)
    {
        auto cosine = cosf((Math::kPi * n * N) - (0.5f * beta));
        return 2.0f * cosine * cosine;
    };

    auto F = [](const float x)
    {
        auto e = std::polar(1.0f, 0.25f * Math::kPi * sqrtf(x / (x + 1.4f)));
        if (x < 0.8f)
        {
            auto term1 = sqrtf(Math::kPi * x);
            auto term2 = 1.0f - (sqrtf(x) / (0.7f * sqrtf(x) + 1.2f));
            return term1 * term2 * e;
        }
        else
        {
            auto term1 = 1.0f - (0.8f / ((x + 1.25f) * (x + 1.25f)));
            return term1 * e;
        }
    };

    auto sign = [](const float x)
    {
        return (x > 0.0f) ? 1 : -1;
    };

    const auto n = 2.0f;
    const auto alpha_i = 0.0f;
    const auto alpha_d = alpha_i + Math::kPi - deviation;
    const auto L = 1.0f;

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        const auto c = PropagationMedium::kSpeedOfSound;
        const auto f = (Bands::kLowCutoffFrequencies[i] + Bands::kHighCutoffFrequencies[i]) / 2.0f;
        const auto l = c / f;
        const auto k = (2.0f * Math::kPi) / l;

        auto D0 = 1.0f / (2.0f * n * sqrtf(2.0f * Math::kPi * k));

        auto e = std::polar(1.0f, -0.25f * Math::kPi);

        auto beta1 = alpha_d - alpha_i;
        auto beta2 = alpha_d - alpha_i;
        auto beta3 = alpha_d + alpha_i;
        auto beta4 = alpha_d + alpha_i;

        auto t1 = cotf((Math::kPi + beta1) / (2.0f * n));
        auto t2 = cotf((Math::kPi - beta2) / (2.0f * n));
        auto t3 = cotf((Math::kPi + beta3) / (2.0f * n));
        auto t4 = cotf((Math::kPi - beta4) / (2.0f * n));

        auto N1 = N_plus(n, beta1);
        auto N2 = N_minus(n, beta2);
        auto N3 = N_plus(n, beta3);
        auto N4 = N_minus(n, beta4);

        auto a1 = a(n, beta1, N1);
        auto a2 = a(n, beta2, N2);
        auto a3 = a(n, beta3, N3);
        auto a4 = a(n, beta4, N4);

        auto x1 = k * L * a1;
        auto x2 = k * L * a2;
        auto x3 = k * L * a3;
        auto x4 = k * L * a4;

        auto F1 = F(x1);
        auto F2 = F(x2);
        auto F3 = F(x3);
        auto F4 = F(x4);

        auto D1 = t1 * F1;
        auto D2 = t2 * F2;
        auto D3 = t3 * F3;
        auto D4 = t4 * F4;

        auto epsilon1 = beta1 - (2.0f * Math::kPi * n * N1) + Math::kPi;
        auto epsilon2 = -(beta2 - (2.0f * Math::kPi * n * N2) - Math::kPi);
        auto epsilon3 = beta3 - (2.0f * Math::kPi * n * N3) + Math::kPi;
        auto epsilon4 = -(beta4 - (2.0f * Math::kPi * n * N4) - Math::kPi);

        if (!Math::isFinite(t1))
        {
            D1 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon1)) - (2 * k * L * epsilon1 * e));
        }
        if (!Math::isFinite(t2))
        {
            D2 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon2)) - (2 * k * L * epsilon2 * e));
        }
        if (!Math::isFinite(t3))
        {
            D3 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon3)) - (2 * k * L * epsilon3 * e));
        }
        if (!Math::isFinite(t4))
        {
            D4 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon4)) - (2 * k * L * epsilon4 * e));
        }

        deviationTerm[i] = abs(D0 * (D1 + D2 + D3 + D4));
    }
}

// For any single SoundPath, we project the corresponding virtual source into Ambisonics and scale the resulting SH
// coefficients by a distance attenuation factor. The SH coefficients for all paths are weighted and summed.
//
// EQ coefficients are averaged.
void PathSimulator::calcAmbisonicsCoeffsForPaths(const Vector3f& source,
                                                 const Vector3f& listener,
                                                 const ProbeBatch& probes,
                                                 int* starts,
                                                 int* ends,
                                                 int numPaths,
                                                 const SoundPath* paths,
                                                 const float* weights,
                                                 int order,
                                                 float* eqGains,
                                                 float* coeffs)
{
    PROFILE_FUNCTION();

    if (eqGains)
    {
        memset(eqGains, 0, Bands::kNumBands * sizeof(float));
        auto numValidPaths = 0;

        for (auto i = 0; i < numPaths; ++i)
        {
            if (!paths[i].isValid())
                continue;

            if (starts[i] >= 0 && ends[i] >= 0)
            {
                float deviationTerm[Bands::kNumBands];
                calcDeviationTerm(std::max(1e-6f, paths[i].deviation(probes, starts[i], ends[i])), deviationTerm);

                float deviationTermReference[Bands::kNumBands];
                calcDeviationTerm(1e-6f, deviationTermReference);
                for (auto j = 0; j < Bands::kNumBands; ++j)
                {
                    deviationTerm[j] /= deviationTermReference[j];
                }

                auto overallGain = 1.0f;
                EQEffect::normalizeGains(deviationTerm, overallGain);

                for (auto j = 0; j < Bands::kNumBands; ++j)
                {
                    eqGains[j] += (overallGain * deviationTerm[j]);
                }
            }
            else
            {
                for (auto j = 0; j < Bands::kNumBands; ++j)
                {
                    eqGains[j] += 1.0f;
                }
            }

            ++numValidPaths;
        }

        if (numValidPaths > 0)
        {
            for (auto i = 0; i < Bands::kNumBands; ++i)
            {
                eqGains[i] /= numValidPaths;
            }
        }
        else
        {
            for (auto i = 0; i < Bands::kNumBands; ++i)
            {
                eqGains[i] = 1.0f;
            }
        }
    }

    if (coeffs)
    {
        auto numCoeffs = SphericalHarmonics::numCoeffsForOrder(order);
        memset(coeffs, 0, numCoeffs * sizeof(float));

        for (auto i = 0; i < numPaths; ++i)
        {
            if (!paths[i].isValid())
                continue;

            Vector3f virtualSource;
            float distance;

            if (starts[i] >= 0 && ends[i] >= 0)
            {
                virtualSource = paths[i].toVirtualSource(probes, starts[i], ends[i]);
                distance = (virtualSource - probes[ends[i]].influence.center).length();
            }
            else
            {
                virtualSource = source;
                distance = (virtualSource - listener).length();
            }

            auto distanceAttenuation = 1.0f / std::max(distance, 1.0f);
            auto gain = weights[i] * distanceAttenuation;

            auto direction = Vector3f::unitVector(virtualSource - listener);
            SphericalHarmonics::projectSinglePointAndUpdate(direction, order, gain, coeffs);
        }
    }
}

void PathSimulator::calcAverageDirectionForPaths(const Vector3f& source,
                                                 const Vector3f& listener,
                                                 const ProbeBatch& probes,
                                                 int* starts,
                                                 int* ends,
                                                 int numPaths,
                                                 const SoundPath* paths,
                                                 const float* weights,
                                                 Vector3f* avgDirection)
{
    PROFILE_FUNCTION();

    if (!avgDirection)
        return;

    Vector3f direction(.0f, .0f, .0f);
    for (auto i = 0; i < numPaths; ++i)
    {
        if (!paths[i].isValid())
            continue;

        Vector3f virtualSource;
        float distance;

        if (starts[i] >= 0 && ends[i] >= 0)
        {
            virtualSource = paths[i].toVirtualSource(probes, starts[i], ends[i]);
            distance = (virtualSource - probes[ends[i]].influence.center).length();
        }
        else
        {
            virtualSource = source;
            distance = (virtualSource - listener).length();
        }

        auto distanceAttenuation = 1.0f / std::max(distance, 1.0f);
        auto gain = weights[i] * distanceAttenuation;
        auto pathDirection = Vector3f::unitVector(virtualSource - listener);

        direction += (pathDirection * gain);
    }

    *avgDirection = Vector3f::unitVector(direction);
}

void PathSimulator::calcDistanceRatioForPaths(const Vector3f& source,
                                              const ProbeBatch& probes,
                                              int* starts,
                                              int* ends,
                                              int numPaths,
                                              const SoundPath* paths,
                                              const float* weights,
                                              float* avgDistanceRatio)
{
    PROFILE_FUNCTION();

    if (!avgDistanceRatio)
        return;

    float ratio = .0f;
    for (auto i = 0; i < numPaths; ++i)
    {
        if (!paths[i].isValid())
            continue;

        float pathRatio = 1.0;

        if (starts[i] >= 0 && ends[i] >= 0)
        {
            Vector3f virtualSource = paths[i].toVirtualSource(probes, source, ends[i]);
            float pathDistance = (virtualSource - probes[ends[i]].influence.center).length();
            float directDistance = (source - probes[ends[i]].influence.center).length();
            pathRatio = (pathDistance > 1.0f && directDistance > 1.0f) ? directDistance / pathDistance : 1.0f;
        }

        ratio += weights[i] * pathRatio;
    }

    // Returns ratio of 1.0 if no valid path is found. Probably should return a large distance ratio instead.
    *avgDistanceRatio = ratio;
}

}
