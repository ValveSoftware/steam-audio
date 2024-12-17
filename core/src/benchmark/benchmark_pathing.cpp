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

#include <profiler.h>
#include <scene_factory.h>
#include <path_simulator.h>
#include <path_effect.h>
#include <sh.h>

using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void benchmarkVisGraphForSettings(IScene const& scene,
                                  const ProbeBatch& probes,
                                  float spacing,
                                  int numSamples,
                                  float range)
{
    auto numRuns = 10;
    auto radius = (numSamples == 1) ? 0.0f : spacing;
    auto threshold = 0.99f;
    std::atomic<bool> cancel(false);

    Timer timer;
    timer.start();

    for (auto i = 0; i < numRuns; ++i)
    {
        ProbeVisibilityTester visTester(numSamples, true, -Vector3f::kYAxis);
        ProbeVisibilityGraph visGraph(scene, probes, visTester, radius, threshold, range, 1, cancel);
    }

    auto msElapsed = timer.elapsedMilliseconds() / numRuns;

    PrintOutput("%-10d %-10.2f %-10.2f\n", numSamples, range, msElapsed);
}

void benchmarkVisGraph(shared_ptr<Context> context,
                       shared_ptr<IScene> scene)
{
    Matrix4x4f localToWorldTransform;
    localToWorldTransform.identity();
    localToWorldTransform *= 80;

    auto spacing = 1.5f;
    auto height = 1.5f;
    ProbeArray probes;
    ProbeGenerator::generateProbes(*scene, localToWorldTransform, ProbeGenerationType::UniformFloor, spacing, height, probes);
    auto numProbes = probes.numProbes();

    auto probeBatch = make_shared<ProbeBatch>();
    probeBatch->addProbeArray(probes);
    probeBatch->commit();

    PrintOutput("Running benchmark: Visibility Graph...\n");
    PrintOutput("%-10s %-10s %-10s\n", "#samples", "range (m)", "time (ms)");

    int numSamplesValues[] = {1, 2, 4, 8};
    float rangeValues[] = {3.0f, 50.0f, INFINITY};
    for (auto numSamples : numSamplesValues)
    {
        for (auto range : rangeValues)
        {
            benchmarkVisGraphForSettings(*scene, *probeBatch, spacing, numSamples, range);
        }
    }

    PrintOutput("\n");
}

void benchmarkPathFindingForSettings(IScene const& scene,
                                     const ProbeBatch& probes,
                                     ProbeVisibilityGraph const& visGraph,
                                     int numSamples,
                                     float radius,
                                     float threshold,
                                     float range)
{
    int numProbes = probes.numProbes();

    int counts[] = {0, 0, 0, 0, 0};
    double times[] = {0.0, 0.0, 0.0, 0.0, 0.0};

    ProbeVisibilityTester visTester(numSamples, true, -Vector3f::kYAxis);
    PathFinder pathFinder(probes, 1);
    const int kProbeSkip = 10;

    for (int i = 0; i < numProbes; i += kProbeSkip)
    {
        for (int j = i + 1; j < numProbes; j += kProbeSkip)
        {
            Timer timer;
            double usElapsed = 0.0;
            ProbePath probePath;

            {
                timer.start();
                probePath = pathFinder.findShortestPath(scene, probes, visGraph, visTester, i, j,
                                                        radius, threshold, range, true, true);
                usElapsed = timer.elapsedMicroseconds();
            }

            int pathLength = std::min(static_cast<int>(probePath.nodes.size()), 4);
            counts[pathLength] += 1;
            times[pathLength] += usElapsed;
        }
    }

    for (int i = 0; i < 5; ++i)
    {
        double avgTime = (counts[i] == 0) ? 0.0 : (times[i] / counts[i]);
        PrintOutput("%-10d %-10.2f %-10d %-10d %-10.2f\n", numSamples, range, i, counts[i], avgTime);
    }
}

void benchmarkPathFinding(shared_ptr<Context> context,
                          shared_ptr<IScene> scene)
{
    Matrix4x4f localToWorldTransform;
    localToWorldTransform.identity();
    localToWorldTransform *= 80;

    auto spacing = 1.5f;
    auto height = 1.5f;
    ProbeArray probes;
    ProbeGenerator::generateProbes(*scene, localToWorldTransform, ProbeGenerationType::UniformFloor, spacing, height, probes);
    auto numProbes = probes.numProbes();

    auto probeBatch = make_shared<ProbeBatch>();
    probeBatch->addProbeArray(probes);
    probeBatch->commit();

    PrintOutput("Running benchmark: Realtime Pathing...\n");
    PrintOutput("%-10s %-10s %-10s %-10s %-10s\n", "#samples", "range (m)", "length", "count", "time (us)");

    int numSamplesValues[] = {1, 2};
    float rangeValues[] = {3.0f, 50.0f, INFINITY};
    std::atomic<bool> cancel(false);

    for (auto numSamples : numSamplesValues)
    {
        for (auto range : rangeValues)
        {
            ProbeVisibilityTester visTester(numSamples, true, -Vector3f::kYAxis);
            ProbeVisibilityGraph visGraph(*scene, *probeBatch, visTester, spacing, 0.99f, range, 1, cancel);
            benchmarkPathFindingForSettings(*scene, *probeBatch, visGraph, numSamples, spacing, 0.99f, range);
        }
    }

    PrintOutput("\n");
}

void benchmarkPathingForSettings(shared_ptr<Context> context, shared_ptr<IScene> scene, int visSamples, int ambisonicsOrder)
{
    Matrix4x4f localToWorldTransform{};
    localToWorldTransform.identity();
    localToWorldTransform *= 80;

    auto spacing = 1.5f;
    auto height = 1.5f;
    ProbeArray probes;
    ProbeGenerator::generateProbes(*scene, localToWorldTransform, ProbeGenerationType::UniformFloor, spacing, height, probes);
    auto numProbes = probes.numProbes();

    auto probeBatch = make_shared<ProbeBatch>();
    probeBatch->addProbeArray(probes);
    probeBatch->commit();

    ProbeManager probeManager;
    probeManager.addProbeBatch(probeBatch);

    const auto kOrder = ambisonicsOrder;
    const auto kNumCoeffs = SphericalHarmonics::numCoeffsForOrder(kOrder);
    auto kNumVisSamples = visSamples;
    auto kProbeVisRadius = (visSamples > 1) ? spacing : .0f;
    const auto kProbeVisThreshold = 0.99f;
    const auto kProbeVisRange = INFINITY;
    const auto kProbePathRange = 5000.0f;
    const auto kNumThreads = 8;
    const auto kListenerVisRadius = kProbeVisRadius;
    const auto kListenerVisThreshold = kProbeVisThreshold;
    const auto kLoadData = false;

    auto pathProgressCallback = [](const float percentComplete,
        void*)
    {
        printf("\rGenerating path data (%3d%%)", static_cast<int>(percentComplete));
    };

    BakedDataIdentifier identifier;
    identifier.variation = BakedDataVariation::Dynamic;
    identifier.type = BakedDataType::Pathing;

    PathBaker::bake(const_cast<const IScene&>(*scene), identifier, kNumVisSamples,
                    kProbeVisRadius, kProbeVisThreshold, kProbeVisRange, kProbeVisRange, kProbePathRange,
                    true, -Vector3f::kYAxis, true, kNumThreads, *probeBatch, pathProgressCallback);
    printf("\r");

    PathSimulator pathSimulator(*probeBatch, kNumVisSamples, true, -Vector3f::kYAxis);
    Array<float, 1> eqGains(Bands::kNumBands);
    Array<float, 1> coeffs(kNumCoeffs);

    float totalTime = .0f;
    int totalProbesBenchmarked = 0;
    const int kProbeSkip = 10;

    for (int i = 0; i < numProbes; i += kProbeSkip)
    {
        for (int j = i + 1; j < numProbes; j += kProbeSkip)
        {
            auto &_source = probes[i].influence.center;
            auto &_listener = probes[j].influence.center;

            Timer timer;
            timer.start();
            {
                ProbeNeighborhood sourceProbes;
                probeManager.getInfluencingProbes(_source, sourceProbes);
                sourceProbes.checkOcclusion(*scene, _source);
                sourceProbes.calcWeights(_source);

                ProbeNeighborhood listenerProbes;
                probeManager.getInfluencingProbes(_listener, listenerProbes);
                listenerProbes.checkOcclusion(*scene, _listener);
                listenerProbes.calcWeights(_listener);

                pathSimulator.findPaths(_source, _listener, *scene, *probeBatch, sourceProbes, listenerProbes, kProbeVisRadius,
                                        kProbeVisThreshold, kProbeVisRange, kOrder, true, true, true, true,
                                        eqGains.data(), coeffs.data());
            }

            totalTime += timer.elapsedMicroseconds();
            ++totalProbesBenchmarked;
        }
    }

    PrintOutput("%-8.2f  %-8d  %-10d  %-8d  %6.2f\n",
        spacing, numProbes, ambisonicsOrder, visSamples, totalTime / totalProbesBenchmarked );
}

BENCHMARK(pathing)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    std::vector<float> vertices;
    std::vector<int32_t> triangleIndices;
    std::vector<int> materialIndices;
    const char* fileName = "../../data/meshes/simplescene.obj";

    LoadObj(fileName, vertices, triangleIndices, materialIndices);
    auto computeDevice = nullptr;
    auto type = SceneType::Default;

    Material material;
    material.absorption[0] = 0.1f;
    material.absorption[1] = 0.1f;
    material.absorption[2] = 0.1f;
    material.scattering = 0.5f;
    material.transmission[0] = 1.0f;
    material.transmission[1] = 1.0f;
    material.transmission[2] = 1.0f;

    auto scene = shared_ptr<IScene>(SceneFactory::create(type, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));

    auto staticMesh = scene->createStaticMesh((int)vertices.size() / 3, (int)triangleIndices.size() / 3, 1,
        reinterpret_cast<Vector3f*>(vertices.data()), (Triangle*) triangleIndices.data(), materialIndices.data(), &material);

    scene->addStaticMesh(staticMesh);
    scene->commit();

    benchmarkVisGraph(context, scene);
    benchmarkPathFinding(context, scene);

    PrintOutput("Running benchmark: Pathing Runtime...\n");
    PrintOutput("%-8s  %-8s  %-10s  %-8s %6s\n", "Spacing", "#Probes", "Ambisonics", "Samples", "(us) Time");

    benchmarkPathingForSettings(context, scene, 1, 0);
    benchmarkPathingForSettings(context, scene, 2, 0);
    benchmarkPathingForSettings(context, scene, 4, 0);
    benchmarkPathingForSettings(context, scene, 8, 0);

    benchmarkPathingForSettings(context, scene, 1, 1);
    benchmarkPathingForSettings(context, scene, 2, 1);
    benchmarkPathingForSettings(context, scene, 4, 1);
    benchmarkPathingForSettings(context, scene, 8, 1);

    benchmarkPathingForSettings(context, scene, 1, 2);
    benchmarkPathingForSettings(context, scene, 2, 2);
    benchmarkPathingForSettings(context, scene, 4, 2);
    benchmarkPathingForSettings(context, scene, 8, 2);

    benchmarkPathingForSettings(context, scene, 1, 3);
    benchmarkPathingForSettings(context, scene, 2, 3);
    benchmarkPathingForSettings(context, scene, 4, 3);
    benchmarkPathingForSettings(context, scene, 8, 3);
}