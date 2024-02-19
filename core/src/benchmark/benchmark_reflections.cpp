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
#include <energy_field_factory.h>
#include <reflection_simulator_factory.h>
#include <scene_factory.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkReflectionsForSettings(shared_ptr<IScene> scene, const SceneType type,
    shared_ptr<OpenCLDevice> openCL, shared_ptr<ipl::RadeonRaysDevice> radeonRays, const int rays, const int bounces, const int sources,
    const float duration, const int order, const int threads)
{
    const int kNumRuns = 1;

    auto simulator = ReflectionSimulatorFactory::create(type, rays, 512, duration, order, sources, 1, threads, 1, radeonRays);

    CoordinateSpace3f listeners[1];
    listeners[0] = CoordinateSpace3f(-Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kZero);

    Array<CoordinateSpace3f> _sources(sources);
    Array<Directivity> directivities(sources);
    Array<unique_ptr<EnergyField>> energyFields(sources);
    Array<EnergyField*> energyFieldPtrs(sources);
    for (auto i = 0; i < sources; ++i)
    {
        _sources[i] = listeners[0];
        directivities[i] = Directivity{};

        energyFields[i] = EnergyFieldFactory::create(type, duration, order, openCL);
        energyFieldPtrs[i] = energyFields[i].get();
    }

    std::atomic<bool> cancel(false);
    ThreadPool threadPool(threads);

    Timer timer;
    timer.start();

    for (auto run = 0; run < kNumRuns; ++run)
    {
        JobGraph jobGraph;
        simulator->simulate(*scene, sources, _sources.data(), 1, listeners, directivities.data(), rays, bounces, duration, order, 1.0f, energyFieldPtrs.data(), jobGraph);
        threadPool.process(jobGraph);

#if defined(IPL_USES_RADEONRAYS)
        if (type == SceneType::RadeonRays)
            clFinish(openCL->irUpdateQueue());
#endif
    }

    auto elapsedTime = timer.elapsedMilliseconds() / kNumRuns;

    PrintOutput("%-10d %10d %10d %10d %8.1f s %10d %8.1f ms\n", rays, bounces, sources, threads, duration, order, elapsedTime);
}

void BenchmarkReflectionsForScene(const std::string& fileName, const SceneType type, const int maxReservedCUs = 0, const float fractionCUIRUpdate = .0f)
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

    // Single thread benchmarking.
    {
        PrintOutput("%-10s %10s %10s %10s %10s %10s %11s\n", "Rays", "Bounces", "Sources", "Threads", "Duration", "Order", "Time");

        auto rays = { 8192, 32768 };
        auto bounces = { 2, 8, 32 };
        auto sources = { 1, 4, 16, 64 };
        auto threads = { 1 };

        for (auto ray : rays)
            for (auto bounce : bounces)
                for (auto source : sources)
                    for (auto thread : threads)
                        BenchmarkReflectionsForSettings(scene, type, openCL, radeonRays, ray, bounce, source, 2.0f, 1, thread);

        PrintOutput("\n");
    }

    // Multi-threaded benchmarking.
    if (type != SceneType::RadeonRays)
    {
        PrintOutput("%-10s %10s %10s %10s %10s %10s %11s\n", "Rays", "Bounces", "Sources", "Threads", "Duration", "Order", "Time");

        auto rays = { 8192, 32768 };
        auto bounces = { 8, 32 };
        auto sources = { 16, 64 };
        auto threads = { 1, 2, 4, 6, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 72  };

        for (auto ray : rays)
            for (auto bounce : bounces)
                for (auto source : sources)
                    for (auto thread : threads)
                        if (thread == 1 || thread * 2 <= static_cast<int>(std::thread::hardware_concurrency()))    // Assumes hyperthreading is turned ON
                            BenchmarkReflectionsForSettings(scene, type, openCL, radeonRays, ray, bounce, source, 2.0f, 1, thread);

        PrintOutput("\n");
    }
}

BENCHMARK(reflections)
{
    SetCoreAffinityForBenchmarking();

    PrintOutput("Running benchmark: Reflection Simulation (Phonon)...\n");
    BenchmarkReflectionsForScene("../../data/meshes/sponza.obj", SceneType::Default);
    PrintOutput("\n");
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    PrintOutput("Running benchmark: Reflection Simulation (Embree)...\n");
    BenchmarkReflectionsForScene("../../data/meshes/sponza.obj", SceneType::Embree);
    PrintOutput("\n");
#endif
#if defined(IPL_USES_RADEONRAYS)
    PrintOutput("Running benchmark: Reflection Simulation (Radeon Rays)...\n");
    BenchmarkReflectionsForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays);
    PrintOutput("\n");
    PrintOutput("Running benchmark: Reflection Simulation (Radeon Rays, 16 CUs)...\n");
    BenchmarkReflectionsForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, 16, 1.0f);
    PrintOutput("\n");
    PrintOutput("Running benchmark: Reflection Simulation (Radeon Rays, 8 CUs)...\n");
    BenchmarkReflectionsForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, 8, 1.0f);
    PrintOutput("\n");
    PrintOutput("Running benchmark: Reflection Simulation (Radeon Rays, 4 CUs)...\n");
    BenchmarkReflectionsForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, 4, 1.0f);
    PrintOutput("\n");
#endif
}
