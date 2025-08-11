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

#include <path_finder.h>
#include <path_visibility.h>
#include <profiler.h>
#include <sampling.h>
#include <scene_factory.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

BENCHMARK(astar)
{
    // -- settings

    auto scene_type = SceneType::Default;
    auto spacing = 3.0f;
    auto height = 1.5f;
    auto num_samples = 1;
    auto asymmetric = false;
    auto down = Vector3f(0.0f, -1.0f, 0.0f);
    auto num_runs = 100;
    auto num_pairs = 1000;
    auto radius = 0.0f;
    auto threshold = 0.99f;
    auto range = 50.0f;
    auto num_threads = 1;

    // -- context

    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    // -- load scene

    std::vector<float> vertices;
    std::vector<int32_t> triangleIndices;
    std::vector<int> materialIndices;
    LoadObj("../../data/meshes/sponza.obj", vertices, triangleIndices, materialIndices);

    Material material;
    for (auto iBand = 0; iBand < Bands::kNumBands; ++iBand)
        material.absorption[iBand] = 0.1f;
    material.scattering = 0.5f;
    for (auto iBand = 0; iBand < Bands::kNumBands; ++iBand)
        material.transmission[iBand] = 1.0f;

    auto scene = shared_ptr<IScene>(SceneFactory::create(scene_type, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));

    auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size() / 3), static_cast<int>(triangleIndices.size() / 3), 1,
                                              reinterpret_cast<Vector3f*>(vertices.data()), (Triangle*) triangleIndices.data(),
                                              materialIndices.data(), &material);

    scene->addStaticMesh(staticMesh);
    scene->commit();

    // -- create probes

    auto transform = Matrix4x4f::identityMatrix();
    transform *= 1000.0f;

    ProbeArray probes{};
    ProbeGenerator::generateProbes(*scene, transform, ProbeGenerationType::UniformFloor, spacing, height, probes);

    auto probe_batch = make_shared<ProbeBatch>();
    probe_batch->addProbeArray(probes);

    // -- create vis tester

    auto vis_tester = make_shared<ProbeVisibilityTester>(num_samples, asymmetric, down);

    // -- vis graph

    std::atomic<bool> cancel(false);
    JobGraph job_graph{};
    auto visgraph = ipl::make_shared<ProbeVisibilityGraph>(*scene, *probe_batch, *vis_tester, radius, threshold, range, num_threads, job_graph, cancel);
    ThreadPool thread_pool(num_threads);
    thread_pool.process(job_graph);
    printf("visgraph: %d nodes\n", probe_batch->numProbes());

    // -- generate pairs

    auto rng = std::default_random_engine{};
    auto first_distribution = std::uniform_int_distribution<int>(0, probe_batch->numProbes() - 1);
    auto second_distribution = std::uniform_int_distribution<int>(1, probe_batch->numProbes() - 1);

    vector<std::pair<int, int>> pairs{};
    for (auto i = 0; i < num_pairs; i++)
    {
        auto first = first_distribution(rng);
        auto second = (first + second_distribution(rng)) % probe_batch->numProbes();
        pairs.push_back(std::make_pair(first, second));
    }

    // -- run

    auto path_finder = make_shared<PathFinder>(*probe_batch, 1);

    auto timer = Timer{};
    timer.start();

    for (auto i = 0; i < num_runs; i++)
    {
        for (auto j = 0; j < num_pairs; j++)
        {
            path_finder->findShortestPath(*scene, *probe_batch, *visgraph, *vis_tester, pairs[j].first, pairs[j].second, radius, threshold, range, false, false);
        }
    }

    auto ms_elapsed = timer.elapsedMilliseconds() / (num_runs * num_pairs);

    printf("%f ms avg\n", ms_elapsed);
}
