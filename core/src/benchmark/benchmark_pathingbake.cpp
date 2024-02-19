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

void benchmarkPathingBakeForSettings(shared_ptr<Context> context, shared_ptr<IScene> scene, int visSamples, int numThreads)
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

    const auto kOrder = 3;
    const auto kNumCoeffs = SphericalHarmonics::numCoeffsForOrder(kOrder);
    auto kNumVisSamples = visSamples;
    auto kProbeVisRadius = (visSamples > 1) ? spacing : .0f;
    const auto kProbeVisThreshold = 0.1f;
    const auto kProbeVisRange = INFINITY;
    const auto kProbePathRange = 5000.0f;
    const auto kNumThreads = numThreads;
    const auto kListenerVisRadius = kProbeVisRadius;
    const auto kListenerVisThreshold = kProbeVisThreshold;
    const auto kLoadData = false;

    auto pathProgressCallback = [](const float percentComplete,
        void*)
    {
        printf("\rGenerating path data (%3d%%)", static_cast<int>(100.0f * percentComplete));
    };

    unique_ptr<BakedPathData> bakedPathData;
    std::atomic<bool> cancel(false);

    ThreadPool threadPool(kNumThreads);

    Timer timer;
    timer.start();
    {
        bakedPathData = ipl::make_unique<BakedPathData>(const_cast<const IScene&>(*scene), *probeBatch, kNumVisSamples,
            kProbeVisRadius, kProbeVisThreshold, kProbeVisRange, kProbeVisRange, kProbePathRange,
            true, -Vector3f::kYAxis, true, kNumThreads, threadPool, cancel, pathProgressCallback);
    }
    auto elapsedSeconds = timer.elapsedSeconds();

    printf("\r");
    PrintOutput("%-8.2f  %-10d  %-12d  %-10d  %-12.2f\n", spacing, numProbes, visSamples, numThreads, elapsedSeconds);
}

BENCHMARK(pathingbake)
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

    PrintOutput("Running benchmark: Pathing Bake...\n");
    PrintOutput("%-8s  %-10s  %-12s  %-10s  %-12s\n", "Spacing", "#Probes", "Vis Samples", "Threads", "Time (sec)");

    benchmarkPathingBakeForSettings(context, scene, 1, 1);
    benchmarkPathingBakeForSettings(context, scene, 1, 2);
    benchmarkPathingBakeForSettings(context, scene, 1, 4);
    benchmarkPathingBakeForSettings(context, scene, 1, 8);

    benchmarkPathingBakeForSettings(context, scene, 2, 1);
    benchmarkPathingBakeForSettings(context, scene, 2, 2);
    benchmarkPathingBakeForSettings(context, scene, 2, 4);
    benchmarkPathingBakeForSettings(context, scene, 2, 8);

    benchmarkPathingBakeForSettings(context, scene, 4, 1);
    benchmarkPathingBakeForSettings(context, scene, 4, 2);
    benchmarkPathingBakeForSettings(context, scene, 4, 4);
    benchmarkPathingBakeForSettings(context, scene, 4, 8);

    benchmarkPathingBakeForSettings(context, scene, 8, 1);
    benchmarkPathingBakeForSettings(context, scene, 8, 2);
    benchmarkPathingBakeForSettings(context, scene, 8, 4);
    benchmarkPathingBakeForSettings(context, scene, 8, 8);
}