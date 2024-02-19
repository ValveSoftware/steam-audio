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

#include "ui_window.h"
#include "helpers.h"
#include <array_math.h>
#include <scene_factory.h>
#include <reflection_simulator_factory.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(instancedmesh)
{
    auto imageWidth = 512;
    auto imageHeight = 512;
    auto sceneType = SceneType::Embree;
    auto numThreads = 4;

    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto embree = (sceneType == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;

    auto scene = loadMesh(context, "sponza.obj", "sponza.mtl", sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree);
    auto sceneCube = loadMesh(context, "smallbox.obj", "smallbox.mtl", sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree);
    auto sceneSphere = loadMesh(context, "sphere.obj", "sphere.mtl", sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree);

    auto x = 0.0f;
    auto y = 5.0f;
    auto z = 0.0f;
    auto cubePosition = Vector3f(x, y, z);
    auto transform = Matrix4x4f{};
    auto instancedCube = scene->createInstancedMesh(sceneCube, transform);

    auto spherePosition = Vector3f(-16.0f, 10.0f, 0.0f);
    auto sphereTransform = Matrix4x4f{};
    sphereTransform.identity();
    sphereTransform(3, 0) = spherePosition.x();
    sphereTransform(3, 1) = spherePosition.y();
    sphereTransform(3, 2) = spherePosition.z();
    auto instancedSphere = scene->createInstancedMesh(sceneSphere, sphereTransform);

    scene->addInstancedMesh(instancedCube);
    scene->addInstancedMesh(instancedSphere);
    scene->commit();

    CoordinateSpace3f sources[2];
    sources[0] = CoordinateSpace3f{-Vector3f::kZAxis, Vector3f::kYAxis, Vector3f{0, -10, 0}};
    sources[1] = CoordinateSpace3f{-Vector3f::kZAxis, Vector3f::kYAxis, Vector3f{-10.5, 1.5, 1.5}};

    CoordinateSpace3f listeners[1];

    Directivity directivities[2];
    directivities[0] = Directivity{};
    directivities[1] = Directivity{};

	auto simulator = ReflectionSimulatorFactory::create(sceneType, imageWidth * imageHeight, 1024, 1.0f, 2, 4, 1, numThreads, 1, nullptr);

    std::atomic<bool> stopSimulation{false};
    JobGraph jobGraph;
    ThreadPool threadPool(numThreads);

    Array<float, 2> image(imageWidth * imageHeight, 4);
    Array<float, 2> accumImage(imageWidth * imageHeight, 4);
    auto numFrames = 0;

    auto numBounces = 1;

    CoordinateSpace3f prevCamera;
    auto prevNumBounces = 0;
    auto prevCubePosition = cubePosition;

    auto gui = [&]()
    {
        ImGui::SliderFloat3("Cube Origin", cubePosition.elements, -10.0f, 10.0f);
        ImGui::SliderInt("Bounces", &numBounces, 1, 4);
    };

    auto display = [&]()
    {
        auto needsReset = (UIWindow::sCamera.origin != prevCamera.origin ||
                           UIWindow::sCamera.ahead != prevCamera.ahead ||
                           UIWindow::sCamera.up != prevCamera.up ||
                           numBounces != prevNumBounces ||
                           cubePosition != prevCubePosition);

        if (needsReset)
            numFrames = 0;

        transform.identity();
        transform(3, 0) = cubePosition.x();
        transform(3, 1) = cubePosition.y();
        transform(3, 2) = cubePosition.z();
        instancedCube->updateTransform(*scene, transform);
        scene->commit();

        listeners[0] = UIWindow::sCamera;

        jobGraph.reset();
        simulator->simulate(*scene, 2, sources, 1, listeners, directivities, imageWidth * imageHeight, numBounces, 1.0f, 2, 1.0f, image, jobGraph);
        threadPool.process(jobGraph);

        if (numFrames == 0)
        {
            memcpy(accumImage.flatData(), image.flatData(), image.totalSize() * sizeof(float));
        }
        else
        {
            ArrayMath::scale(static_cast<int>(image.totalSize()), accumImage.flatData(), static_cast<float>(numFrames), accumImage.flatData());
            ArrayMath::add(static_cast<int>(image.totalSize()), accumImage.flatData(), image.flatData(), accumImage.flatData());
            ArrayMath::scale(static_cast<int>(image.totalSize()), accumImage.flatData(), 1.0f / (numFrames + 1.0f), accumImage.flatData());
        }

        ++numFrames;
        UIWindow::drawImage(accumImage.flatData(), imageWidth, imageHeight);

        prevCamera = UIWindow::sCamera;
        prevNumBounces = numBounces;
        prevCubePosition = cubePosition;
    };

    UIWindow window;
    window.run(gui, display, nullptr);
}
