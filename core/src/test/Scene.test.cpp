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

#include <scene.h>

TEST_CASE("IScene", "[IScene]")
{
}

TEST_CASE("Scene", "[Scene]")
{
}

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#include <embree_device.h>
#include <scene_factory.h>

using namespace ipl;

TEST_CASE("Embree scene sees an instanced mesh added after another one was removed and released.", "[Scene]")
{
    auto embree = make_shared<EmbreeDevice>();
    auto createScene = [&]()
    {
        return shared_ptr<IScene>(SceneFactory::create(SceneType::Embree, nullptr, nullptr, nullptr, nullptr, nullptr, embree, nullptr));
    };

    Vector3f vertices[] = { Vector3f{ -1.0f, -1.0f, -1.0f }, Vector3f{ 1.0f, -1.0f, -1.0f }, Vector3f{ 0.0f, 1.0f, -1.0f } };
    Triangle triangles[1];
    triangles[0].indices[0] = 0;
    triangles[0].indices[1] = 1;
    triangles[0].indices[2] = 2;
    int materialIndices[] = { 0 };
    Material materials[1] = {};

    auto scene = createScene();
    auto createInstance = [&]()
    {
        auto subScene = createScene();
        subScene->addStaticMesh(subScene->createStaticMesh(3, 1, 1, vertices, triangles, materialIndices, materials));
        subScene->commit();
        return scene->createInstancedMesh(subScene, Matrix4x4f::identityMatrix());
    };

    auto first = createInstance();
    scene->addInstancedMesh(first);
    scene->commit();
    scene->removeInstancedMesh(first);
    scene->commit();
    first = nullptr;

    auto second = createInstance();
    scene->addInstancedMesh(second);
    scene->commit();

    Ray ray{ Vector3f::kZero, -Vector3f::kZAxis };
    REQUIRE(scene->closestHit(ray, 0.0f, 10.0f).isValid());
}

#endif
