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
#include <impulse_response_factory.h>
#include <reconstructor_factory.h>
#include <reflection_simulator_factory.h>
#include <scene_factory.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkReconstructionForSettings(std::shared_ptr<IScene> scene, const SceneType type, const IndirectEffectType convType,
    shared_ptr<OpenCLDevice> openCL, shared_ptr<ipl::RadeonRaysDevice> radeonRays, const int sources, const float duration, const int order)
{
    const int kNumRuns = 1;

    auto simulator = ReflectionSimulatorFactory::create(type, 8192, 4096, duration, order, sources, 1, 1, 1, radeonRays);
    auto reconstructor = ReconstructorFactory::create(type, convType, duration, order, 48000, radeonRays);

    CoordinateSpace3f listeners[1];
    listeners[0] = CoordinateSpace3f(-Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kZero);

    Array<CoordinateSpace3f> _sources(sources);
    Array<float*> distanceCurves(sources);
    Array<AirAbsorptionModel> airAbsorptions(sources);
    Array<Directivity> directivities(sources);
    Array<unique_ptr<EnergyField>> energyFields(sources);
    Array<unique_ptr<ImpulseResponse>> impulseResponses(sources);
    Array<EnergyField*> energyFieldPtrs(sources);
    Array<ImpulseResponse*> impulseResponsePtrs(sources);
    for (auto i = 0; i < sources; ++i)
    {
        _sources[i] = CoordinateSpace3f(-Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kYAxis);
        distanceCurves[i] = nullptr;
        airAbsorptions[i] = AirAbsorptionModel{};
        directivities[i] = Directivity{};

        energyFields[i] = EnergyFieldFactory::create(type, duration, order, openCL);
        energyFieldPtrs[i] = energyFields[i].get();

        impulseResponses[i] = ImpulseResponseFactory::create(convType, duration, order, 48000, openCL);
        impulseResponsePtrs[i] = impulseResponses[i].get();
    }

    ThreadPool threadPool(1);

    JobGraph jobGraph;
    simulator->simulate(*scene, sources, _sources.data(), 1, listeners, directivities.data(), 8192, 4, duration, order, 1.0f, energyFieldPtrs.data(), jobGraph);
    threadPool.process(jobGraph);

#if defined(IPL_USES_RADEONRAYS)
    if (type == SceneType::RadeonRays)
        clFinish(openCL->irUpdateQueue());
#endif

    Timer timer;
    timer.start();

    for (auto run = 0; run < kNumRuns; ++run)
    {
        reconstructor->reconstruct(sources, energyFieldPtrs.data(), distanceCurves.data(), airAbsorptions.data(), impulseResponsePtrs.data(), ReconstructionType::Gaussian, duration, order);

#if defined(IPL_USES_RADEONRAYS)
        if (type == SceneType::RadeonRays)
            clFinish(openCL->irUpdateQueue());
#endif
    }

    auto elapsedTime = timer.elapsedMilliseconds() / kNumRuns;

    PrintOutput("%-10d %8.1f s %10d %8.1f ms\n", sources, duration, order, elapsedTime);
}

void BenchmarkReconstructionForScene(const std::string& fileName, const SceneType type, const IndirectEffectType convType, const int maxReservedCUs = 0, const float fractionCUIRUpdate = .0f )
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

    auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size() / 3), static_cast<int>(triangleIndices.size() / 3), 1,
                                              reinterpret_cast<Vector3f*>(vertices.data()), (Triangle*) triangleIndices.data(),
                                              materialIndices.data(), &material);

    scene->addStaticMesh(staticMesh);
    scene->commit();

    for (auto sources = 1; sources <= 64; sources *= 4)
        for (auto duration = 0.5f; duration <= 4.0f; duration *= 2.0f)
            for (auto order = 0; order <= 3; ++order)
                BenchmarkReconstructionForSettings(scene, type, convType, openCL, radeonRays, sources, duration, order);
}

BENCHMARK(reconstruction)
{
    PrintOutput("Running benchmark: Reconstruction (CPU)...\n");
    PrintOutput("%-10s %10s %10s %10s\n", "#Sources", "Duration", "Order", "Time");
    BenchmarkReconstructionForScene("../../data/meshes/sponza.obj", SceneType::Default, IndirectEffectType::Convolution);
    PrintOutput("\n");
#if defined(IPL_USES_RADEONRAYS)
    PrintOutput("Running benchmark: Reconstruction (OpenCL)...\n");
    BenchmarkReconstructionForScene("../../data/meshes/sponza.obj", SceneType::RadeonRays, IndirectEffectType::TrueAudioNext);
    PrintOutput("\n");
    PrintOutput( "Running benchmark: Reconstruction (OpenCL, 16 CUs)...\n" );
    BenchmarkReconstructionForScene( "../../data/meshes/sponza.obj", SceneType::RadeonRays, IndirectEffectType::TrueAudioNext, 16, 1.0f );
    PrintOutput( "\n" );
    PrintOutput( "Running benchmark: Reconstruction (OpenCL, 8 CUs)...\n" );
    BenchmarkReconstructionForScene( "../../data/meshes/sponza.obj", SceneType::RadeonRays, IndirectEffectType::TrueAudioNext, 8, 1.0f );
    PrintOutput( "\n" );
    PrintOutput( "Running benchmark: Reconstruction (OpenCL, 4 CUs)...\n" );
    BenchmarkReconstructionForScene( "../../data/meshes/sponza.obj", SceneType::RadeonRays, IndirectEffectType::TrueAudioNext, 4, 1.0f );
    PrintOutput( "\n" );
#endif
}
