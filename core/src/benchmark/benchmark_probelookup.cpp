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

using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

enum LookUpMode {
    NEAREST,
    ALL
};

void benchmarkProbeLookupForSettings(shared_ptr<Context> context, shared_ptr<IScene> scene, float spacing, LookUpMode mode)
{
    Matrix4x4f localToWorldTransform{};
    localToWorldTransform.identity();
    localToWorldTransform *= 8000;

    auto height = 1.5f;
    ProbeArray probes;
    ProbeGenerator::generateProbes(*scene, localToWorldTransform, ProbeGenerationType::UniformFloor, spacing, height, probes);
    auto numProbes = probes.numProbes();

    auto probeBatch = make_shared<ProbeBatch>();
    probeBatch->addProbeArray(probes);
    probeBatch->commit();

    ProbeManager probeManager;
    probeManager.addProbeBatch(probeBatch);

    PathSimulator pathSimulator(*probeBatch, 1, true, -Vector3f::kYAxis);

    int kNumRuns = 1000;
    int probeIndex = numProbes / 2;
    Vector3f queryPosition = probes[probeIndex].influence.center;

    Timer timer;
    timer.start();
    {
        ProbeNeighborhood neighborhood;

        if (mode == NEAREST)
        {
            for (int i = 0; i < kNumRuns; ++i)
            {
                probeManager.getInfluencingProbes(queryPosition, neighborhood);
                neighborhood.checkOcclusion(*scene, queryPosition);
                auto nearest = neighborhood.findNearest(queryPosition);
            }
        }
        else if (mode == ALL)
        {
            for (int i = 0; i < kNumRuns; ++i)
            {
                probeManager.getInfluencingProbes(queryPosition, neighborhood);
                neighborhood.checkOcclusion(*scene, queryPosition);
                neighborhood.calcWeights(queryPosition);
            }
        }
    }
    auto elapsedTime = timer.elapsedMicroseconds() / kNumRuns;

    printf("\r");
    PrintOutput("%-10s %-8.2f %-10d %-10.2f\n", mode == NEAREST ? "Nearest" : "All", spacing, numProbes, elapsedTime);
}

BENCHMARK(probelookup)
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

    PrintOutput("%s...\n", fileName);
    PrintOutput("Running benchmark: Probe Lookup...\n");
    PrintOutput("%-10s %-8s %-10s %-12s\n", "Mode", "Spacing", "#Probes", "Time (us)");

    benchmarkProbeLookupForSettings(context, scene, 2.5f, LookUpMode::NEAREST);
    benchmarkProbeLookupForSettings(context, scene, 2.0f, LookUpMode::NEAREST);
    benchmarkProbeLookupForSettings(context, scene, 1.5f, LookUpMode::NEAREST);
    benchmarkProbeLookupForSettings(context, scene, 1.0f, LookUpMode::NEAREST);

    benchmarkProbeLookupForSettings(context, scene, 2.5f, LookUpMode::ALL);
    benchmarkProbeLookupForSettings(context, scene, 2.0f, LookUpMode::ALL);
    benchmarkProbeLookupForSettings(context, scene, 1.5f, LookUpMode::ALL);
    benchmarkProbeLookupForSettings(context, scene, 1.0f, LookUpMode::ALL);
}