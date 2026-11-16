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

#include <direct_simulator.h>
#include <scene_factory.h>
using namespace ipl;

namespace {

// A single large wall perpendicular to the X axis.
shared_ptr<IScene> makeWallScene(float x)
{
    Vector3f vertices[] = {
        Vector3f(x, -50.0f, -50.0f), Vector3f(x, 50.0f, -50.0f), Vector3f(x, 50.0f, 50.0f), Vector3f(x, -50.0f, 50.0f)
    };
    Triangle triangles[] = { Triangle{ { 0, 1, 2 } }, Triangle{ { 0, 2, 3 } } };
    int materialIndices[] = { 0, 0 };
    Material material{};
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        material.absorption[i] = 0.1f;
        material.transmission[i] = 0.1f;
    }
    material.scattering = 0.5f;

    auto scene = shared_ptr<IScene>(SceneFactory::create(SceneType::Default, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    scene->addStaticMesh(scene->createStaticMesh(4, 2, 1, vertices, triangles, materialIndices, &material));
    scene->commit();
    return scene;
}

float volumetricOcclusion(const IScene& scene, int maxNumSamples, int numSamples)
{
    DirectSimulator simulator(maxNumSamples);
    DirectSoundPath path{};
    simulator.simulate(&scene, CalcOcclusion, CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f(10.0f, 0.0f, 0.0f) },
                       CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f::kZero }, DistanceAttenuationModel{},
                       AirAbsorptionModel{}, Directivity{}, OcclusionType::Volumetric, 0.5f, numSamples, 1, path);
    return path.occlusion;
}

}

TEST_CASE("Volumetric occlusion of an unobstructed source is 1 for any sample count", "[DirectSimulator]")
{
    // The wall is behind the listener, so nothing lies between the listener and the source.
    auto scene = makeWallScene(-5.0f);

    for (auto maxNumSamples : { 1, 2, 16 })
    {
        auto occlusion = volumetricOcclusion(*scene, maxNumSamples, 1);
        INFO("maxNumOcclusionSamples " << maxNumSamples << ", 1 sample: occlusion " << occlusion);
        CHECK(occlusion == 1.0f);
    }
}
