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

#include <catch.hpp>

#include <atomic>
#include <cmath>
#include <vector>

#include <air_absorption.h>
#include <embree_device.h>
#include <energy_field.h>
#include <job_graph.h>
#include <reflection_simulator_factory.h>
#include <reverb_estimator.h>
#include <scene_factory.h>

using namespace ipl;

namespace {

// Simulates reflections in a closed, uniformly absorbing box (the source and listener are inside it) and returns
// the mid-band reverberation time that Steam Audio's own estimator reads from the resulting energy field.
float simulatedReverbTime(SceneType sceneType,
                          float sizeX,
                          float sizeY,
                          float sizeZ,
                          float absorption,
                          float scattering,
                          int numRays,
                          int numBounces,
                          float duration)
{
    auto embree = (sceneType == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;
    auto scene = shared_ptr<IScene>(SceneFactory::create(sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree, nullptr));

    Material material;
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        material.absorption[i] = absorption;
        material.transmission[i] = 0.0f;
    }
    material.scattering = scattering;

    Vector3f vertices[8];
    for (auto i = 0; i < 8; ++i)
    {
        vertices[i] = Vector3f((i & 1) ? 0.5f * sizeX : -0.5f * sizeX,
                               (i & 2) ? 0.5f * sizeY : -0.5f * sizeY,
                               (i & 4) ? 0.5f * sizeZ : -0.5f * sizeZ);
    }

    const int quads[6][4] = { {0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 5, 7, 6} };
    Triangle triangles[12];
    int materialIndices[12] = {};
    for (auto i = 0; i < 6; ++i)
    {
        triangles[2 * i + 0] = Triangle{ {quads[i][0], quads[i][1], quads[i][2]} };
        triangles[2 * i + 1] = Triangle{ {quads[i][0], quads[i][2], quads[i][3]} };
    }

    scene->addStaticMesh(scene->createStaticMesh(8, 12, 1, vertices, triangles, materialIndices, &material));
    scene->commit();

    auto simulator = ReflectionSimulatorFactory::create(sceneType, numRays, 1024, duration, 0, 1, 1, 1, 64, nullptr);

    CoordinateSpace3f source(Vector3f(0.23f * sizeX, 0.17f * sizeY, 0.12f * sizeZ));
    CoordinateSpace3f listener(Vector3f(-0.2f * sizeX, -0.13f * sizeY, -0.08f * sizeZ));
    Directivity directivity;

    EnergyField energyField(duration, 0);
    EnergyField* energyFields[] = { &energyField };

    JobGraph jobGraph;
    simulator->simulate(*scene, 1, &source, 1, &listener, &directivity, numRays, numBounces, duration, 0, 1.0f,
                        energyFields, jobGraph);

    std::atomic<bool> cancel(false);
    while (jobGraph.processNextJob(0, cancel)) {}

    Reverb reverb;
    ReverbEstimator::estimate(energyField, AirAbsorptionModel(std::vector<float>(Bands::kNumBands, 0.0f).data(), nullptr, nullptr), reverb);
    return reverb.reverbTimes[1];
}

float eyringReverbTime(float sizeX,
                       float sizeY,
                       float sizeZ,
                       float absorption)
{
    auto volume = sizeX * sizeY * sizeZ;
    auto area = 2.0f * (sizeX * sizeY + sizeY * sizeZ + sizeX * sizeZ);
    return 0.161f * volume / (-area * logf(1.0f - absorption));
}

}

TEST_CASE("ReflectionSimulator", "[ReflectionSimulator]")
{
}

TEST_CASE("DecoupledReflectionSimulator", "[DecoupledReflectionSimulator]")
{
}

TEST_CASE("Simulated reverb time in a closed box matches Eyring's formula (CPU)", "[ReflectionSimulatorBox]")
{
    auto expected = eyringReverbTime(3.0f, 3.0f, 2.5f, 0.05f);
    auto actual = simulatedReverbTime(SceneType::Default, 3.0f, 3.0f, 2.5f, 0.05f, 1.0f, 16384, 600, 2.0f);
    INFO("Eyring: " << expected << " s, simulated: " << actual << " s");
    REQUIRE(fabsf(actual - expected) < 0.12f * expected);
}

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

TEST_CASE("Simulated reverb time in a closed box matches Eyring's formula (Embree)", "[ReflectionSimulatorBox]")
{
    auto expected = eyringReverbTime(3.0f, 3.0f, 2.5f, 0.05f);
    auto actual = simulatedReverbTime(SceneType::Embree, 3.0f, 3.0f, 2.5f, 0.05f, 1.0f, 16384, 600, 2.0f);
    INFO("Eyring: " << expected << " s, simulated: " << actual << " s");
    REQUIRE(fabsf(actual - expected) < 0.12f * expected);
}

#endif
