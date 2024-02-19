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
#include <probe_generator.h>
#include <reflection_baker.h>
#include <reflection_simulator_factory.h>
#include <scene_factory.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkBakingForSettings(shared_ptr<Context> context, shared_ptr<IScene> scene, const SceneType type,
        shared_ptr<OpenCLDevice> openCL, shared_ptr<ipl::RadeonRaysDevice> radeonRays, byte_t* probeData, size_t probeDataSize,
        const float spacing, const int rays, const int diffuseSamples, const int bounces, const int threads)
{
    auto simulator = ReflectionSimulatorFactory::create(type, rays, diffuseSamples, 2.0f, 1, 1, 1, threads, 1, radeonRays);

    SerializedObject serializedObject(probeDataSize, probeData);
    ProbeBatch probeBatch(serializedObject);

    BakedDataIdentifier identifier;
    identifier.variation = BakedDataVariation::Reverb;
    identifier.type = BakedDataType::Reflections;
    identifier.endpointInfluence = Sphere{};

    if (type == SceneType::RadeonRays)
    {
        ReflectionBaker::bake(*scene, *simulator, identifier, true, false, rays, bounces, 2.0f, 2.0f, 1, 1.0f, threads, 1, type, openCL, probeBatch);
#if defined(IPL_USES_RADEONRAYS)
        if (type == SceneType::RadeonRays)
            clFinish(openCL->irUpdateQueue());
#endif
    }

    Timer timer;
    timer.start();
    {
        ReflectionBaker::bake(*scene, *simulator, identifier, true, false, rays, bounces, 2.0f, 2.0f, 1, 1.0f, threads, 1, type, openCL, probeBatch);
#if defined(IPL_USES_RADEONRAYS)
        if (type == SceneType::RadeonRays)
            clFinish(openCL->irUpdateQueue());
#endif
    }
    auto elapsedSeconds = timer.elapsedSeconds();

    PrintOutput("%-6d  %10d  %10d  %8.2f  %10d  %10d %8.2f\n", rays, diffuseSamples, bounces, spacing, probeBatch.numProbes(), threads,
        elapsedSeconds);
}

uint64_t GetProbeData(const std::string& fileName, const float spacing,
                      const int rays, const int diffuseSamples, const int bounces, const int threads,
                      byte_t** probeData)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    std::vector<float>   vertices;
    std::vector<int32_t> triangleIndices;
    std::vector<int>     materialIndices;

    LoadObj(fileName, vertices, triangleIndices, materialIndices);

    float xmin = std::numeric_limits<float>().max();
    float ymin = std::numeric_limits<float>().max();
    float zmin = std::numeric_limits<float>().max();
    float xmax = std::numeric_limits<float>().lowest();
    float ymax = std::numeric_limits<float>().lowest();
    float zmax = std::numeric_limits<float>().lowest();

    for (auto i = 0u; i < vertices.size(); i += 3)
    {
        xmin = std::min(xmin, vertices[i]);
        ymin = std::min(ymin, vertices[i + 1]);
        zmin = std::min(zmin, vertices[i + 2]);

        xmax = std::max(xmax, vertices[i]);
        ymax = std::max(ymax, vertices[i + 1]);
        zmax = std::max(zmax, vertices[i + 2]);
    }

    Matrix4x4f localToWorldTransform;
    localToWorldTransform.identity();
    localToWorldTransform(0, 3) = (xmin + xmax) / 2;
    localToWorldTransform(1, 3) = (ymin + ymax) / 2;
    localToWorldTransform(2, 3) = (zmin + zmax) / 2;
    localToWorldTransform(0, 0) = (xmax - xmin);
    localToWorldTransform(1, 1) = (ymax - ymin);
    localToWorldTransform(2, 2) = (zmax - zmin);

    Material material;
    material.absorption[0] = 0.1f;
    material.absorption[1] = 0.1f;
    material.absorption[2] = 0.1f;
    material.scattering = 0.5f;
    material.transmission[0] = 1.0f;
    material.transmission[1] = 1.0f;
    material.transmission[2] = 1.0f;

    auto scene = shared_ptr<IScene>(SceneFactory::create(SceneType::Default, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));

    auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size() / 3), static_cast<int>(triangleIndices.size() / 3), 1,
                                              reinterpret_cast<Vector3f*>(vertices.data()), (Triangle*) triangleIndices.data(),
                                              materialIndices.data(), &material);

    scene->addStaticMesh(staticMesh);
    scene->commit();

    ProbeArray probes;
    ProbeGenerator::generateProbes(*scene, localToWorldTransform, ProbeGenerationType::UniformFloor, spacing, 1.5f, probes);

    ProbeBatch probeBatch;
    for (auto i = 0u; i < probes.probes.size(0); ++i)
    {
        probeBatch.addProbe(probes.probes[i].influence);
    }

    probeBatch.commit();

    if (*probeData != nullptr)
        free(*probeData);

    SerializedObject probeSerializedObject;
    probeBatch.serializeAsRoot(probeSerializedObject);
    *probeData = (byte_t*) malloc(static_cast<size_t>(probeSerializedObject.size()));
    memcpy(*probeData, probeSerializedObject.data(), static_cast<size_t>(probeSerializedObject.size()));

    return probeSerializedObject.size();
}

void BenchmarkBakingForScene(const std::string& fileName, const SceneType type, const int maxReservedCUs = 0, const float fractionCUIRUpdate = .0f)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);
    byte_t* probeData = nullptr;

    std::vector<float>   vertices;
    std::vector<int32_t> triangleIndices;
    std::vector<int>     materialIndices;

    LoadObj(fileName, vertices, triangleIndices, materialIndices);

    auto embree = (type == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;
#if defined(IPL_USES_OPENCL)
    auto openCLDeviceList = (type == SceneType::RadeonRays) ? make_shared<OpenCLDeviceList>(OpenCLDeviceType::GPU, maxReservedCUs, fractionCUIRUpdate, false) : nullptr;
    auto openCL = (type == SceneType::RadeonRays) ? make_shared<OpenCLDevice>((*openCLDeviceList)[0].platform, (*openCLDeviceList)[0].device, 0, 0) : nullptr;
    auto radeonRays = (type == SceneType::RadeonRays) ? ipl::make_shared<ipl::RadeonRaysDevice>(openCL) : nullptr;
#else
    auto openCL = nullptr;
    auto radeonRays = nullptr;
#endif

    Material material;
    material.absorption[0] = 0.1f;
    material.absorption[1] = 0.1f;
    material.absorption[2] = 0.1f;
    material.scattering = 0.5f;
    material.transmission[0] = 1.0f;
    material.transmission[1] = 1.0f;
    material.transmission[2] = 1.0f;

    auto scene = shared_ptr<IScene>(SceneFactory::create(type, nullptr, nullptr, nullptr, nullptr, nullptr, embree, radeonRays));

    auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size() / 3), static_cast<int>(triangleIndices.size() / 3), 1,
                                              reinterpret_cast<Vector3f*>(vertices.data()), (Triangle*) triangleIndices.data(),
                                              materialIndices.data(), &material);

    scene->addStaticMesh(staticMesh);
    scene->commit();

    // Single thread benchmarking.
    {
        PrintOutput("%-6s  %10s  %10s  %10s  %10s  %10s  %10s\n", "Rays", "Diffuse", "Bounces", "Spacing", "#Probes", "Threads", "Time (sec)");

        {
            auto probeDataSize = GetProbeData(fileName, 8.0f, 32768, 512, 4, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 8.0f, 32768, 512, 4, 1);
        }

        {
            auto probeDataSize = GetProbeData(fileName, 3.71f, 32768, 512, 4, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 3.71f, 32768, 512, 4, 1);
        }

        {
            auto probeDataSize = GetProbeData(fileName, 3.71f, 16384, 512, 4, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 3.71f, 16384, 512, 4, 1);
        }

        {
            auto probeDataSize = GetProbeData(fileName, 3.71f, 16384, 512, 2, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 3.71f, 16384, 512, 2, 1);
        }
        {
            auto probeDataSize = GetProbeData(fileName, 8.0f, 32768, 512, 64, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 8.0f, 32768, 512, 64, 1);
        }

        {
            auto probeDataSize = GetProbeData(fileName, 3.71f, 32768, 512, 64, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 3.71f, 32768, 512, 64, 1);
        }

        {
            auto probeDataSize = GetProbeData(fileName, 3.71f, 16384, 512, 64, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 3.71f, 16384, 512, 64, 1);
        }

        {
            auto probeDataSize = GetProbeData(fileName, 3.71f, 16384, 512, 32, 1, &probeData);
            BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 3.71f, 16384, 512, 32, 1);
        }

        PrintOutput("\n");
    }

    // Multi-threaded benchmarking.
    if (type != SceneType::RadeonRays)
    {
        PrintOutput("%-6s  %10s  %10s  %10s  %10s  %10s  %10s\n", "Rays", "Diffuse", "Bounces", "Spacing", "#Probes", "Threads", "Time (s)");

        auto threads = { 1, 2, 4, 6, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 72 };
        for (auto thread : threads)
            if (thread == 1 || thread * 2 <= static_cast<int>(std::thread::hardware_concurrency()))    // Assumes hyperthreading is turned ON
            {
                auto probeDataSize = GetProbeData(fileName, 8.0f, 16384, 512, 64, thread, &probeData);
                BenchmarkBakingForSettings(context, scene, type, openCL, radeonRays, probeData, static_cast<size_t>(probeDataSize), 8.0f, 32768, 512, 64, thread);
            }

        PrintOutput("\n");
    }
}

BENCHMARK(baking)
{
    SetCoreAffinityForBenchmarking();

    PrintOutput("Running benchmark: Baking Simulation (Phonon)...\n");
    BenchmarkBakingForScene("../../data/meshes/sponza.obj", SceneType::Default);
    PrintOutput("\n");
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    PrintOutput("Running benchmark: Baking Simulation (Embree)...\n");
    BenchmarkBakingForScene("../../data/meshes/sponza.obj", SceneType::Embree);
    PrintOutput("\n");
#endif
#if defined(IPL_USES_RADEONRAYS)
    PrintOutput("Running benchmark: Baking Simulation (Radeon Rays, all CUs)...\n");
    BenchmarkBakingForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, 0, 1.0f);
    PrintOutput("\n");
    //PrintOutput("Running benchmark: Baking Simulation (Radeon Rays, 16 CUs)...\n");
    //BenchmarkBakingForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, 16, 1.0f);
    //PrintOutput("\n");
    //PrintOutput("Running benchmark: Baking Simulation (Radeon Rays, 8 CUs)...\n");
    //BenchmarkBakingForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, 8. 1.0f);
    //PrintOutput("\n");
    //PrintOutput("Running benchmark: Baking Simulation (Radeon Rays, 4 CUs)...\n");
    //BenchmarkBakingForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, 4, 1.0f);
    //PrintOutput("\n");
#endif
}