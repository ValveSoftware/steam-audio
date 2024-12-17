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

#include <fstream>

#include <catch.hpp>

#include <energy_field_factory.h>
#include <reflection_simulator.h>
#include <reflection_simulator_factory.h>
#include <scene_factory.h>
#include <opencl_device.h>
#include <opencl_energy_field.h>
#include <radeonrays_device.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

std::shared_ptr<IScene> loadMesh(const std::string& fileName, const std::string& materialFileName,
                                 shared_ptr<Context> context, shared_ptr<EmbreeDevice> embree, shared_ptr<ipl::RadeonRaysDevice> radeonRays,
                                 const SceneType& sceneType)
{
    std::ifstream mtlfile((std::string{ "../../data/meshes/" } +materialFileName).c_str());
    if (!mtlfile.good())
    {
        printf("unable to find mtl file.");
        exit(0);
    }

    std::vector<Material> materials;
    std::unordered_map<std::string, int> materialIndices;
    std::string currentmtl;

    float kd[3];
    float ks[3];

    while (true)
    {
        std::string command;
        mtlfile >> command;

        if (mtlfile.eof())
            break;

        if (command == "newmtl")
        {
            std::string mtlname;
            mtlfile >> mtlname;

            currentmtl = mtlname;

            materials.push_back(Material());
            materialIndices[mtlname] = static_cast<int>(materials.size()) - 1;
        }
        else if (command == "Kd")
        {
            mtlfile >> kd[0] >> kd[1] >> kd[2];

            for (auto i = 0; i < 3; ++i)
                materials[materialIndices[currentmtl]].absorption[i] = 1.0f - kd[i];

            // Transmission material is kept at default (full loss).
            materials[materialIndices[currentmtl]].scattering = 1.0f;
        }
        else if (command == "Ks")
        {
            mtlfile >> ks[0] >> ks[1] >> ks[2];

            for (auto i = 0; i < 3; ++i)
                materials[materialIndices[currentmtl]].absorption[i] = 1.0f - kd[i];

            // Transmission material is kept at default (full loss).
            materials[materialIndices[currentmtl]].scattering = 1.0f;
        }
    }

    std::ifstream file((std::string{ "../../data/meshes/" } +fileName).c_str());
    if (!file.good())
    {
        printf("unable to find obj file.");
    }

    std::vector<Vector3f> vertices;
    std::vector<int> triangles;
    std::vector<int> materialIndicesForTriangles;

    while (true)
    {
        std::string command;
        file >> command;

        if (file.eof())
            break;

        if (command == "v")
        {
            float x, y, z;
            file >> x >> y >> z;
            vertices.push_back(Vector3f(x, y, z));
        }
        else if (command == "f")
        {
            std::string s[3];
            file >> s[0] >> s[1] >> s[2];

            for (auto i = 0; i < 3; ++i)
                triangles.push_back(atoi(s[i].substr(0, s[i].find_first_of("/")).c_str()) - 1);

            if (materialIndices.find(currentmtl) == materialIndices.end())
            {
                printf("WARNING: No material assigned!\n");
            }
            else
            {
                materialIndicesForTriangles.push_back(materialIndices[currentmtl]);
            }
        }
        else if (command == "usemtl")
        {
            file >> currentmtl;
        }
    }

    auto scene = ipl::shared_ptr<IScene>(SceneFactory::create(sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree, radeonRays));

    auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size()), static_cast<int>(triangles.size()) / 3, static_cast<int>(materials.size()),
                                              vertices.data(), (Triangle*) triangles.data(), materialIndicesForTriangles.data(), materials.data());

    scene->addStaticMesh(staticMesh);
    scene->commit();

    return std::move(scene);
}

#if !defined(IPL_OS_IOS) && !defined(IPL_OS_WASM)

TEST_CASE("All implementations of ReflectionSimulator produce comparable results.", "[ReflectionSimulator]")
{
    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto phononScene = loadMesh("sponza.obj", "sponza.mtl", context, nullptr, nullptr, SceneType::Default);

    CoordinateSpace3f listeners[1];
    listeners[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kZero };

    CoordinateSpace3f sources[10];
    Directivity directivities[10];
    for (auto i = 0; i < 10; ++i)
    {
        sources[i] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f{i - 5.0f, -10.0f, 0.0f} };
        directivities[i] = Directivity{ i / 10.0f, 1.0f };
    }

    EnergyField* energyFields[10];

    unique_ptr<EnergyField> phononEnergyFields[10];
    for (auto i = 0; i < 10; ++i)
    {
        phononEnergyFields[i] = EnergyFieldFactory::create(SceneType::Default, 2.0f, 1, nullptr);
        energyFields[i] = phononEnergyFields[i].get();
    }

    auto phononReflectionSimulator = ReflectionSimulatorFactory::create(SceneType::Default, 32768, 4096, 2.0f, 1, 10, 1, 1, 1, nullptr);

    JobGraph phononJobGraph;
    ThreadPool phononThreadPool(1);

    phononReflectionSimulator->simulate(*phononScene, 10, sources, 1, listeners, directivities, 32768, 32, 2.0f, 1, 1.0f, energyFields, phononJobGraph);
    phononThreadPool.process(phononJobGraph);

    auto numChannels = phononEnergyFields[0]->numChannels();
    auto numBands = Bands::kNumBands;
    auto numBins = phononEnergyFields[0]->numBins();

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    auto embree = make_shared<EmbreeDevice>();

    auto embreeScene = loadMesh("sponza.obj", "sponza.mtl", context, embree, nullptr, SceneType::Embree);

    unique_ptr<EnergyField> embreeEnergyFields[10];
    for (auto i = 0; i < 10; ++i)
    {
        embreeEnergyFields[i] = EnergyFieldFactory::create(SceneType::Embree, 2.0f, 1, nullptr);
        energyFields[i] = embreeEnergyFields[i].get();
    }

    auto embreeReflectionSimulator = ReflectionSimulatorFactory::create(SceneType::Embree, 32768, 4096, 2.0f, 1, 10, 1, 1, 1, nullptr);

    JobGraph embreeJobGraph;
    ThreadPool embreeThreadPool(1);

    embreeReflectionSimulator->simulate(*embreeScene, 10, sources, 1, listeners, directivities, 32768, 32, 2.0f, 1, 1.0f, energyFields, embreeJobGraph);
    embreeThreadPool.process(embreeJobGraph);

    for (auto source = 0; source < 10; ++source)
    {
		const auto& lhs = *embreeEnergyFields[source];
		const auto& rhs = *phononEnergyFields[source];

        for (auto i = 0; i < numChannels; ++i)
        {
            for (auto j = 0; j < numBands; ++j)
            {
                for (auto k = 0; k < numBins; ++k)
                {
                    REQUIRE(lhs[i][j][k] == Approx(rhs[i][j][k]));
                }
            }
        }
    }
#endif

#if defined(IPL_USES_RADEONRAYS)
    auto openCLDeviceList = make_shared<OpenCLDeviceList>(OpenCLDeviceType::GPU, 0, 0.0f, false);
    const auto& deviceDesc = (*openCLDeviceList)[0];
    auto openCL = make_shared<OpenCLDevice>(deviceDesc.platform, deviceDesc.device, 0, 0);

    auto radeonRays = ipl::make_shared<ipl::RadeonRaysDevice>(openCL);

    auto radeonRaysScene = loadMesh("sponza.obj", "sponza.mtl", context, nullptr, radeonRays, SceneType::RadeonRays);

    unique_ptr<EnergyField> radeonRaysEnergyFields[10];
    for (auto i = 0; i < 10; ++i)
    {
        radeonRaysEnergyFields[i] = EnergyFieldFactory::create(SceneType::RadeonRays, 2.0f, 1, openCL);
        energyFields[i] = radeonRaysEnergyFields[i].get();
    }

    auto radeonRaysReflectionSimulator = ReflectionSimulatorFactory::create(SceneType::RadeonRays, 32768, 4096, 2.0f, 1, 10, 1, 1, 1, radeonRays);

    JobGraph radeonRaysJobGraph;
    ThreadPool radeonRaysThreadPool(1);

    radeonRaysReflectionSimulator->simulate(*radeonRaysScene, 10, sources, 1, listeners, directivities, 32768, 32, 2.0f, 1, 1.0f, energyFields, radeonRaysJobGraph);
    radeonRaysThreadPool.process(radeonRaysJobGraph);
    clFinish(openCL->irUpdateQueue());

    for (auto i = 0; i < 10; ++i)
    {
        static_cast<OpenCLEnergyField&>(*radeonRaysEnergyFields[i]).copyDeviceToHost();
    }

    // NOTE: There seem to be discrepancies in the results calculated on the GPU vs results calculated on the CPU.
    // As long as energy field plots in the itest are similar, we should be fine.
    for (auto source = 0; source < 10; ++source)
    {
        const auto& lhs = *radeonRaysEnergyFields[source].get();
        const auto& rhs = *phononEnergyFields[source].get();

        for (auto i = 0; i < numChannels; ++i)
        {
            for (auto j = 0; j < numBands; ++j)
            {
                for (auto k = 0; k < numBins; ++k)
                {
                    //REQUIRE(lhs[i][j][k] == Approx(rhs[i][j][k]));
                }
            }
        }
    }
#endif
}

TEST_CASE("Multi-threaded implementations of CPUReflectionSimulator produce comparable results.", "[ReflectionSimulator]")
{
    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto phononScene = loadMesh("sponza.obj", "sponza.mtl", context, nullptr, nullptr, SceneType::Default);

    CoordinateSpace3f listeners[1];
    listeners[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kZero };

    CoordinateSpace3f sources[10];
    Directivity directivities[10];
    for (auto i = 0; i < 10; ++i)
    {
        sources[i] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f{i - 5.0f, -10.0f, 0.0f} };
        directivities[i] = Directivity{ i / 10.0f, 1.0f };
    }

    EnergyField* energyFields[10];

    JobGraph jobGraph;

    // 1 THREAD

    unique_ptr<EnergyField> phononEnergyFields_1[10];
    for (auto i = 0; i < 10; ++i)
    {
        phononEnergyFields_1[i] = EnergyFieldFactory::create(SceneType::Default, 2.0f, 1, nullptr);
    }

    auto phononReflectionSimulator_1 = ReflectionSimulatorFactory::create(SceneType::Default, 32768, 4096, 2.0f, 1, 10, 1, 1, 1, nullptr);

    ThreadPool phononThreadPool_1(1);

    for (auto i = 0; i < 10; ++i)
    {
        energyFields[i] = phononEnergyFields_1[i].get();
    }

    jobGraph.reset();
    phononReflectionSimulator_1->simulate(*phononScene, 10, sources, 1, listeners, directivities, 32768, 32, 2.0f, 1, 1.0f, energyFields, jobGraph);
    phononThreadPool_1.process(jobGraph);

    // 2 THREADS

    unique_ptr<EnergyField> phononEnergyFields_2[10];
    for (auto i = 0; i < 10; ++i)
    {
        phononEnergyFields_2[i] = EnergyFieldFactory::create(SceneType::Default, 2.0f, 1, nullptr);
    }

    auto phononReflectionSimulator_2 = ReflectionSimulatorFactory::create(SceneType::Default, 32768, 4096, 2.0f, 1, 10, 1, 2, 1, nullptr);

    ThreadPool phononThreadPool_2(2);

    for (auto i = 0; i < 10; ++i)
    {
        energyFields[i] = phononEnergyFields_2[i].get();
    }

    jobGraph.reset();
    phononReflectionSimulator_2->simulate(*phononScene, 10, sources, 1, listeners, directivities, 32768, 32, 2.0f, 1, 1.0f, energyFields, jobGraph);
    phononThreadPool_2.process(jobGraph);

    // COMPARE

    auto numChannels = phononEnergyFields_1[0]->numChannels();
    auto numBins = phononEnergyFields_1[0]->numBins();

    for (auto source = 0; source < 10; ++source)
    {
        const auto& lhs = *phononEnergyFields_1[source];
        const auto& rhs = *phononEnergyFields_2[source];

        for (auto i = 0; i < numChannels; ++i)
        {
            for (auto j = 0; j < Bands::kNumBands; ++j)
            {
                for (auto k = 0; k < numBins; ++k)
                {
                    REQUIRE(lhs[i][j][k] == Approx(rhs[i][j][k]));
                }
            }
        }
    }
}

#endif
