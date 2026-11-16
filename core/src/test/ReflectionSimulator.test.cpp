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

#include <embree_device.h>
#include <energy_field.h>
#include <job_graph.h>
#include <reflection_simulator_factory.h>
#include <scene_factory.h>

using namespace ipl;

namespace {

const float kBoxSize[3] = { 3.0f, 3.0f, 2.5f };
const float kSourcePosition[3] = { 0.7f, 0.5f, 0.3f };
const float kListenerPosition[3] = { -0.6f, -0.4f, -0.2f };

// Energy that reaches the listener through specular reflections that bounce off the walls of a box (centered at the
// origin) exactly once or twice, as given by the image source method: each image of the source gives
// (1 - absorption)^(number of reflections) / (4 pi d^2), where d is the distance from the image to the listener.
float imageSourceEnergy(float absorption)
{
    const auto pi = 3.14159265f;

    auto total = 0.0f;
    for (auto nx = -2; nx <= 2; ++nx) for (auto px = 0; px <= 1; ++px)
    for (auto ny = -2; ny <= 2; ++ny) for (auto py = 0; py <= 1; ++py)
    for (auto nz = -2; nz <= 2; ++nz) for (auto pz = 0; pz <= 1; ++pz)
    {
        const int n[3] = { nx, ny, nz };
        const int p[3] = { px, py, pz };

        auto numReflections = 0;
        auto distanceSquared = 0.0f;
        for (auto i = 0; i < 3; ++i)
        {
            numReflections += abs(2 * n[i] - p[i]);

            // Wall positions are at +/- size / 2, so the images are laid out relative to a corner of the box.
            auto source = kSourcePosition[i] + 0.5f * kBoxSize[i];
            auto listener = kListenerPosition[i] + 0.5f * kBoxSize[i];
            auto image = 2.0f * n[i] * kBoxSize[i] + (p[i] ? -source : source);
            distanceSquared += (image - listener) * (image - listener);
        }

        if (numReflections < 1 || numReflections > 2)
            continue;

        total += powf(1.0f - absorption, static_cast<float>(numReflections)) / (4.0f * pi * distanceSquared);
    }
    return total;
}

// Simulates two bounces in a box whose walls are perfect (but absorbing) mirrors, and returns the total energy
// (omnidirectional, mid-band) of the resulting energy field.
float simulatedEnergy(SceneType sceneType,
                      float absorption,
                      int numRays)
{
    const auto pi = 3.14159265f;
    const auto duration = 0.2f;

    auto embree = (sceneType == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;
    auto scene = shared_ptr<IScene>(SceneFactory::create(sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree, nullptr));

    Material material;
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        material.absorption[i] = absorption;
        material.transmission[i] = 0.0f;
    }
    material.scattering = 0.0f;

    Vector3f vertices[8];
    for (auto i = 0; i < 8; ++i)
    {
        vertices[i] = Vector3f((i & 1) ? 0.5f * kBoxSize[0] : -0.5f * kBoxSize[0],
                               (i & 2) ? 0.5f * kBoxSize[1] : -0.5f * kBoxSize[1],
                               (i & 4) ? 0.5f * kBoxSize[2] : -0.5f * kBoxSize[2]);
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

    CoordinateSpace3f source(Vector3f(kSourcePosition[0], kSourcePosition[1], kSourcePosition[2]));
    CoordinateSpace3f listener(Vector3f(kListenerPosition[0], kListenerPosition[1], kListenerPosition[2]));
    Directivity directivity;

    EnergyField energyField(duration, 0);
    EnergyField* energyFields[] = { &energyField };

    JobGraph jobGraph;
    simulator->simulate(*scene, 1, &source, 1, &listener, &directivity, numRays, 2, duration, 0, 0.01f, energyFields,
                        jobGraph);

    std::atomic<bool> cancel(false);
    while (jobGraph.processNextJob(0, cancel)) {}

    auto total = 0.0f;
    for (auto i = 0; i < energyField.numBins(); ++i)
    {
        total += energyField[0][1][i];
    }

    // The order-0 spherical harmonic is 1 / (2 sqrt(pi)).
    return total * 2.0f * sqrtf(pi);
}

}

TEST_CASE("ReflectionSimulator", "[ReflectionSimulator]")
{
}

TEST_CASE("DecoupledReflectionSimulator", "[DecoupledReflectionSimulator]")
{
}

TEST_CASE("Specular reflections carry the energy of the image sources (CPU)", "[ReflectionSimulatorSpecular]")
{
    auto expected = imageSourceEnergy(0.05f);
    auto actual = simulatedEnergy(SceneType::Default, 0.05f, 65536);
    INFO("image sources: " << expected << ", simulated: " << actual);
    REQUIRE(fabsf(actual - expected) < 0.08f * expected);
}

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

TEST_CASE("Specular reflections carry the energy of the image sources (Embree)", "[ReflectionSimulatorSpecular]")
{
    auto expected = imageSourceEnergy(0.05f);
    auto actual = simulatedEnergy(SceneType::Embree, 0.05f, 65536);
    INFO("image sources: " << expected << ", simulated: " << actual);
    REQUIRE(fabsf(actual - expected) < 0.08f * expected);
}

#endif
