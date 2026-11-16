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
#include <map>
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

// A closed, diffuse-walled sphere of the given radius (a subdivided icosahedron). It has no corners, so the only
// thing that can make its reverberation time differ from Eyring's formula is how the simulator treats the paths.
void makeSphere(float radius,
                int subdivisions,
                std::vector<Vector3f>& vertices,
                std::vector<Triangle>& triangles)
{
    const auto t = (1.0f + sqrtf(5.0f)) / 2.0f;
    const float base[12][3] = { {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
                                {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1} };
    const int faces[20][3] = { {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2},
                               {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5},
                               {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1} };

    for (auto i = 0; i < 12; ++i)
    {
        vertices.push_back(Vector3f::unitVector(Vector3f(base[i][0], base[i][1], base[i][2])));
    }

    std::vector<Triangle> current;
    for (auto i = 0; i < 20; ++i)
    {
        current.push_back(Triangle{ {faces[i][0], faces[i][1], faces[i][2]} });
    }

    for (auto level = 0; level < subdivisions; ++level)
    {
        std::map<std::pair<int, int>, int> midpoints;
        auto midpoint = [&](int a, int b)
        {
            auto key = std::make_pair(std::min(a, b), std::max(a, b));
            auto it = midpoints.find(key);
            if (it != midpoints.end())
                return it->second;
            vertices.push_back(Vector3f::unitVector(vertices[a] + vertices[b]));
            return midpoints[key] = static_cast<int>(vertices.size()) - 1;
        };

        std::vector<Triangle> next;
        for (const auto& tri : current)
        {
            auto a = tri.indices[0], b = tri.indices[1], c = tri.indices[2];
            auto ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            next.push_back(Triangle{ {a, ab, ca} });
            next.push_back(Triangle{ {b, bc, ab} });
            next.push_back(Triangle{ {c, ca, bc} });
            next.push_back(Triangle{ {ab, bc, ca} });
        }
        current = next;
    }

    for (auto& vertex : vertices)
    {
        vertex = vertex * radius;
    }
    triangles = current;
}

// Mid-band reverberation time that Steam Audio's own estimator reads from the energy field of a source and a
// listener inside a sphere.
float simulatedReverbTime(SceneType sceneType,
                          float radius,
                          float absorption,
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
    material.scattering = 1.0f;

    std::vector<Vector3f> vertices;
    std::vector<Triangle> triangles;
    makeSphere(radius, 3, vertices, triangles);
    std::vector<int> materialIndices(triangles.size(), 0);

    scene->addStaticMesh(scene->createStaticMesh(static_cast<int>(vertices.size()), static_cast<int>(triangles.size()), 1,
                                                 vertices.data(), triangles.data(), materialIndices.data(), &material));
    scene->commit();

    auto simulator = ReflectionSimulatorFactory::create(sceneType, numRays, 1024, duration, 0, 1, 1, 1, 64, nullptr);

    CoordinateSpace3f source(Vector3f(0.4f * radius, 0.2f * radius, 0.1f * radius));
    CoordinateSpace3f listener(Vector3f(-0.3f * radius, -0.2f * radius, -0.1f * radius));
    Directivity directivity;

    EnergyField energyField(duration, 0);
    EnergyField* energyFields[] = { &energyField };

    JobGraph jobGraph;
    simulator->simulate(*scene, 1, &source, 1, &listener, &directivity, numRays, numBounces, duration, 0, 0.25f,
                        energyFields, jobGraph);

    std::atomic<bool> cancel(false);
    while (jobGraph.processNextJob(0, cancel)) {}

    Reverb reverb;
    ReverbEstimator::estimate(energyField, AirAbsorptionModel(std::vector<float>(Bands::kNumBands, 0.0f).data(), nullptr, nullptr), reverb);
    return reverb.reverbTimes[1];
}

// 4V / S is the mean free path of a closed convex room; Eyring's formula follows from the energy lost per bounce.
float eyringReverbTime(float radius,
                       float absorption)
{
    const auto pi = 3.14159265f;
    auto volume = (4.0f / 3.0f) * pi * radius * radius * radius;
    auto area = 4.0f * pi * radius * radius;
    return 0.161f * volume / (-area * logf(1.0f - absorption));
}

}

TEST_CASE("ReflectionSimulator", "[ReflectionSimulator]")
{
}

TEST_CASE("DecoupledReflectionSimulator", "[DecoupledReflectionSimulator]")
{
}

// A point source and a listener do not absorb sound, so their 10 cm spheres must not stop the paths that cross them.
TEST_CASE("Source and listener do not absorb sound in a closed sphere (CPU)", "[ReflectionSimulatorSphere]")
{
    auto expected = eyringReverbTime(1.0f, 0.02f);
    auto actual = simulatedReverbTime(SceneType::Default, 1.0f, 0.02f, 16384, 1200, 4.0f);
    INFO("Eyring: " << expected << " s, simulated: " << actual << " s");
    REQUIRE(fabsf(actual - expected) < 0.1f * expected);
}

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

TEST_CASE("Source and listener do not absorb sound in a closed sphere (Embree)", "[ReflectionSimulatorSphere]")
{
    auto expected = eyringReverbTime(1.0f, 0.02f);
    auto actual = simulatedReverbTime(SceneType::Embree, 1.0f, 0.02f, 16384, 1200, 4.0f);
    INFO("Eyring: " << expected << " s, simulated: " << actual << " s");
    REQUIRE(fabsf(actual - expected) < 0.1f * expected);
}

#endif
