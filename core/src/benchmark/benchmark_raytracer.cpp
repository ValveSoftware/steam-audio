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
#include <reflection_simulator_factory.h>
#include <scene_factory.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkRaytracerForSettings(shared_ptr<IScene> scene, const SceneType type,
    shared_ptr<OpenCLDevice> openCL, shared_ptr<ipl::RadeonRaysDevice> radeonRays, const int bounces, const int threads)
{
    const int kNumRuns = (type == SceneType::RadeonRays) ? 10 : 1;

    auto imageWidth = 512 * 4;
    auto imageHeight = 512 * 4;

    auto simulator = ReflectionSimulatorFactory::create(type, imageWidth * imageHeight, 1024, 0.1f, 0, 2, 1, threads, 1, radeonRays);

    CoordinateSpace3f sources[2];
    sources[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f{ 0, -10, 0 } };
    sources[1] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f{ 10, 0, 0 } };

    CoordinateSpace3f listeners[1];
    listeners[0] = CoordinateSpace3f(-Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kZero);

    Directivity directivities[2];
    directivities[0] = Directivity{ 0.0f, 10.0f, nullptr, nullptr };
    directivities[1] = Directivity{ 0.0f, 10.0f, nullptr, nullptr };

    Array<float, 2> image(imageWidth * imageHeight, 4);

    std::atomic<bool> stopSimulation{ false };
    ThreadPool threadPool(threads);

    if (type == SceneType::RadeonRays)
    {
        for (auto run = 0; run < kNumRuns; ++run)
        {
            JobGraph jobGraph;
            simulator->simulate(*scene, 2, sources, 1, listeners, directivities, imageWidth * imageHeight, bounces, 0.1f, 0, 1.0f, image, jobGraph);
            threadPool.process(jobGraph);

#if defined(IPL_USES_OPENCL)
            if (type == SceneType::RadeonRays)
                clFinish(openCL->irUpdateQueue());
#endif
        }
    }

    Timer timer;
    timer.start();

    for (auto run = 0; run < kNumRuns; ++run)
    {
        JobGraph jobGraph;
        simulator->simulate(*scene, 2, sources, 1, listeners, directivities, imageWidth * imageHeight, bounces, 0.1f, 0, 1.0f, image, jobGraph);
        threadPool.process(jobGraph);

#if defined(IPL_USES_OPENCL)
        if (type == SceneType::RadeonRays)
            clFinish(openCL->irUpdateQueue());
#endif
    }

    auto elapsedMilliseconds = timer.elapsedMilliseconds() / kNumRuns;
    auto mrps = (2 * bounces * imageWidth * imageHeight * 1e-3) / elapsedMilliseconds;

    PrintOutput("%dx%d %10d %10d %10d %8.1f\n", imageWidth, imageHeight, bounces, 2, threads, mrps);
}

void BenchmarkRaytracerForScene(const std::string& fileName, const SceneType type, const int maxReservedCUs = 0, const float fractionCUIRUpdate = .0f)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    std::vector<float>   vertices;
    std::vector<int32_t> triangleIndices;
    std::vector<int>     materialIndices;

    LoadObj(fileName, vertices, triangleIndices, materialIndices);

    Material material;
    material.absorption[0] = 0.1f;
    material.absorption[1] = 0.1f;
    material.absorption[2] = 0.1f;
    material.scattering = 0.5f;
    material.transmission[0] = 1.0f;
    material.transmission[1] = 1.0f;
    material.transmission[2] = 1.0f;

    auto embree = (type == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;
#if defined(IPL_USES_OPENCL)
    auto openCLDeviceList = (type == SceneType::RadeonRays) ? make_shared<OpenCLDeviceList>(OpenCLDeviceType::GPU, 0, 0.0f, false) : nullptr;
    auto openCL = (type == SceneType::RadeonRays) ? make_shared<OpenCLDevice>((*openCLDeviceList)[0].platform, (*openCLDeviceList)[0].device, 0, 0) : nullptr;
    auto radeonRays = (type == SceneType::RadeonRays) ? ipl::make_shared<ipl::RadeonRaysDevice>(openCL) : nullptr;
#else
    auto openCL = nullptr;
    auto radeonRays = nullptr;
#endif
    auto scene = shared_ptr<IScene>(SceneFactory::create(type, nullptr, nullptr, nullptr, nullptr, nullptr, embree, radeonRays));

    auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size()) / 3, static_cast<int>(triangleIndices.size()) / 3, 1,
                                           reinterpret_cast<Vector3f*>(vertices.data()), (Triangle*) triangleIndices.data(),
                                           materialIndices.data(), &material);

    scene->addStaticMesh(staticMesh);
    scene->commit();

    {
        PrintOutput("%-10s %10s %10s %10s %11s\n", "Rays", "Bounces", "Sources", "Threads", "Mrps");

        auto bounces = { 1, 2, 4 };
        for (auto bounce : bounces)
            BenchmarkRaytracerForSettings(scene, type, openCL, radeonRays, bounce, 1);

        PrintOutput("\n");
    }

    if (type != SceneType::RadeonRays)
    {
        PrintOutput("%-10s %10s %10s %10s %11s\n", "Rays", "Bounces", "Sources", "Threads", "Mrps");

        auto bounces = { 2 };
        auto threads = { 1, 2, 4, 6, 8, 12, 16, 20, 24, 28, 32 };

        for (auto bounce : bounces)
            for (auto thread: threads)
                if (thread == 1 || thread * 2 <= static_cast<int>(std::thread::hardware_concurrency()))    // Assumes hyperthreading is turned ON
                    BenchmarkRaytracerForSettings(scene, type, openCL, radeonRays, bounce, thread);
    }
}

BENCHMARK(raytracer)
{
    SetCoreAffinityForBenchmarking();

    PrintOutput("Running benchmark: Raytracer (Phonon)...\n");
    BenchmarkRaytracerForScene("../../data/meshes/sponza.obj", SceneType::Default);
    PrintOutput("\n");
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    PrintOutput("Running benchmark: Raytracer (Embree)...\n");
    BenchmarkRaytracerForScene("../../data/meshes/sponza.obj", SceneType::Embree);
    PrintOutput("\n");
#endif
#if defined(IPL_USES_RADEONRAYS)
    PrintOutput("Running benchmark: Raytracer (Radeon Rays, all CUs)...\n");
    BenchmarkRaytracerForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays);
    PrintOutput("\n");
    //PrintOutput("Running benchmark: Raytracer (Radeon Rays, 16 CUs)...\n");
    //BenchmarkRaytracerForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, true, 16);
    //PrintOutput("\n");
    //PrintOutput("Running benchmark: Raytracer (Radeon Rays, 8 CUs)...\n");
    //BenchmarkRaytracerForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, true, 8);
    //PrintOutput("\n");
    //PrintOutput("Running benchmark: Raytracer (Radeon Rays, 4 CUs)...\n");
    //BenchmarkRaytracerForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, true, 4);
    //PrintOutput("\n");
#endif
}
