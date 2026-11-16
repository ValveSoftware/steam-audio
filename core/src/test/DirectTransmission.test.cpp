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

#include <vector>

#include <catch.hpp>

#include <delay_effect.h>
#include <direct_effect.h>
#include <direct_simulator.h>
#include <eq_effect.h>
#include <scene_factory.h>
using namespace ipl;

namespace {

// Walls perpendicular to the X axis. Each wall is two parallel quads (a solid slab), as in a closed mesh.
struct Wall
{
    float x;
    float thickness;
    float transmission;
};

shared_ptr<IScene> makeScene(SceneType type, const std::vector<Wall>& walls)
{
    std::vector<Vector3f> vertices;
    std::vector<Triangle> triangles;
    std::vector<int> materialIndices;
    std::vector<Material> materials;

    for (const auto& wall : walls)
    {
        Material material{};
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            material.transmission[i] = wall.transmission;
            material.absorption[i] = 0.1f;
        }
        material.scattering = 0.5f;
        materials.push_back(material);

        for (auto face = 0; face < 2; ++face)
        {
            auto x = (face == 0) ? wall.x : wall.x + wall.thickness;
            auto base = static_cast<int>(vertices.size());
            vertices.push_back(Vector3f(x, -50.0f, -50.0f));
            vertices.push_back(Vector3f(x, 50.0f, -50.0f));
            vertices.push_back(Vector3f(x, 50.0f, 50.0f));
            vertices.push_back(Vector3f(x, -50.0f, 50.0f));
            triangles.push_back(Triangle{ { base, base + 1, base + 2 } });
            triangles.push_back(Triangle{ { base, base + 2, base + 3 } });
            materialIndices.push_back(static_cast<int>(materials.size()) - 1);
            materialIndices.push_back(static_cast<int>(materials.size()) - 1);
        }
    }

    shared_ptr<EmbreeDevice> embree;
    if (type == SceneType::Embree)
        embree = make_shared<EmbreeDevice>();

    auto scene = shared_ptr<IScene>(SceneFactory::create(type, nullptr, nullptr, nullptr, nullptr, nullptr, embree, nullptr));
    auto mesh = scene->createStaticMesh(static_cast<int>(vertices.size()), static_cast<int>(triangles.size()),
                                        static_cast<int>(materials.size()), vertices.data(), triangles.data(),
                                        materialIndices.data(), materials.data());
    scene->addStaticMesh(mesh);
    scene->commit();
    return scene;
}

float transmissionThrough(const IScene& scene, int numRays)
{
    DirectSimulator simulator(1);
    DirectSoundPath path{};
    simulator.simulate(&scene, CalcTransmission, CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f(10.0f, 0.0f, 0.0f) },
                       CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kZero }, DistanceAttenuationModel{},
                       AirAbsorptionModel{}, Directivity{}, OcclusionType::Raycast, 0.0f, 1, numRays, path);
    return path.transmission[1];
}

}

TEST_CASE("Transmission through one solid wall does not depend on the number of rays", "[DirectSimulator]")
{
    for (auto type : { SceneType::Default, SceneType::Embree })
    {
        auto scene = makeScene(type, { Wall{ 4.0f, 0.2f, 0.1f } });
        for (auto numRays : { 1, 2, 3, 4, 8, 16 })
        {
            auto t = transmissionThrough(*scene, numRays);
            INFO("scene " << static_cast<int>(type) << ", " << numRays << " rays: transmission " << t << " (wall: 0.1)");
            CHECK(t == Approx(0.1f).epsilon(0.01));
        }
    }
}

TEST_CASE("Transmission through two solid walls is the product, once each, for 4 or more rays", "[DirectSimulator]")
{
    auto scene = makeScene(SceneType::Default, { Wall{ 3.0f, 0.2f, 0.5f }, Wall{ 6.0f, 0.2f, 0.2f } });
    for (auto numRays : { 4, 5, 6, 8, 16 })
    {
        auto t = transmissionThrough(*scene, numRays);
        INFO(numRays << " rays: transmission " << t << " (walls: 0.5 * 0.2 = 0.1)");
        CHECK(t == Approx(0.1f).epsilon(0.01));
    }
}
