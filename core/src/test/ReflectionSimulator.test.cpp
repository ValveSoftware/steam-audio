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

TEST_CASE("ReflectionSimulator", "[ReflectionSimulator]")
{
}

TEST_CASE("DecoupledReflectionSimulator", "[DecoupledReflectionSimulator]")
{
}

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

namespace {

// Total mid-band energy of the energy field from two bounces in a box (3 x 3 x 2.5 m before any rotation) whose walls
// are perfect, absorbing mirrors. If the box is instanced, it is rotated by 90 degrees about the y axis, which makes
// it 2.5 x 3 x 3 m; otherwise it is built directly with those dimensions.
float simulatedEnergy(bool instanced)
{
    const auto numRays = 16384;
    const auto duration = 0.2f;

    auto embree = make_shared<EmbreeDevice>();
    auto createScene = [&]()
    {
        return shared_ptr<IScene>(SceneFactory::create(SceneType::Embree, nullptr, nullptr, nullptr, nullptr, nullptr, embree, nullptr));
    };

    Material material;
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        material.absorption[i] = 0.05f;
        material.transmission[i] = 0.0f;
    }
    material.scattering = 0.0f;

    const float size[3] = { instanced ? 3.0f : 2.5f, 3.0f, instanced ? 2.5f : 3.0f };
    Vector3f vertices[8];
    for (auto i = 0; i < 8; ++i)
    {
        vertices[i] = Vector3f((i & 1) ? 0.5f * size[0] : -0.5f * size[0],
                               (i & 2) ? 0.5f * size[1] : -0.5f * size[1],
                               (i & 4) ? 0.5f * size[2] : -0.5f * size[2]);
    }

    const int quads[6][4] = { {0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 5, 7, 6} };
    Triangle triangles[12];
    int materialIndices[12] = {};
    for (auto i = 0; i < 6; ++i)
    {
        triangles[2 * i + 0] = Triangle{ {quads[i][0], quads[i][1], quads[i][2]} };
        triangles[2 * i + 1] = Triangle{ {quads[i][0], quads[i][2], quads[i][3]} };
    }

    auto scene = createScene();
    auto subScene = createScene();
    subScene->addStaticMesh(subScene->createStaticMesh(8, 12, 1, vertices, triangles, materialIndices, &material));
    subScene->commit();

    if (instanced)
    {
        // (x, y, z) -> (z, y, -x)
        auto rotation = Matrix4x4f::identityMatrix();
        rotation(0, 0) = 0.0f;
        rotation(0, 2) = 1.0f;
        rotation(2, 0) = -1.0f;
        rotation(2, 2) = 0.0f;
        scene->addInstancedMesh(scene->createInstancedMesh(subScene, rotation));
    }
    else
    {
        scene->addStaticMesh(scene->createStaticMesh(8, 12, 1, vertices, triangles, materialIndices, &material));
    }
    scene->commit();

    auto simulator = ReflectionSimulatorFactory::create(SceneType::Embree, numRays, 1024, duration, 0, 1, 1, 1, 64, nullptr);

    CoordinateSpace3f source(Vector3f(0.5f, 0.5f, 0.3f));
    CoordinateSpace3f listener(Vector3f(-0.6f, -0.4f, -0.2f));
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
    return total;
}

}

TEST_CASE("Reflections in a rotated instanced mesh match those in the same geometry built directly", "[ReflectionSimulatorInstanced]")
{
    auto expected = simulatedEnergy(false);
    auto actual = simulatedEnergy(true);
    INFO("direct: " << expected << ", instanced: " << actual);
    REQUIRE(expected > 0.0f);
    REQUIRE(fabsf(actual - expected) < 0.03f * expected);
}

#endif
