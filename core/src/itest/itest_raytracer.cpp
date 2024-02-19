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
#include <profiler.h>
#include <reflection_simulator_factory.h>
#include <scene_factory.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(raytracer)
{
    const auto objFileName = "sponza.obj";
    const auto mtlFileName = "sponza.mtl";
    const auto imageWidth = 512;
    const auto imageHeight = 512;
    const auto numThreads = 8;

    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

	auto embree = make_shared<EmbreeDevice>();
#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
    auto openCLDeviceList = make_shared<OpenCLDeviceList>(OpenCLDeviceType::GPU, 0, 0.0f, false);
	auto openCL = make_shared<OpenCLDevice>((*openCLDeviceList)[0].platform, (*openCLDeviceList)[0].device, 0, 0);
	auto radeonRays = ipl::make_shared<ipl::RadeonRaysDevice>(openCL);
#endif

    map<SceneType, shared_ptr<IScene>> sceneForType;
    sceneForType[SceneType::Default] = loadMesh(context, objFileName, mtlFileName, SceneType::Default);

    CoordinateSpace3f sources[2];
    sources[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f{ 0, -10, 0 } };
    sources[1] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, Vector3f{ 0, 0, 0 } };

    CoordinateSpace3f listeners[1];

    Directivity directivities[2];
    directivities[0] = Directivity{ 0.0f, 10.0f, nullptr, nullptr };
    directivities[1] = Directivity{ 0.0f, 10.0f, nullptr, nullptr };

    map<SceneType, shared_ptr<IReflectionSimulator>> simulatorForType;
    simulatorForType[SceneType::Default] = ReflectionSimulatorFactory::create(SceneType::Default, imageWidth * imageHeight, 1024, 0.1f, 0, 2, 1, numThreads, 1, nullptr);

    std::atomic<bool> stopSimulation{ false };
    JobGraph jobGraph;
    ThreadPool threadPool(numThreads);

    Array<float, 2> image(imageWidth * imageHeight, 4);
    Array<float, 2> accumImage(imageWidth * imageHeight, 4);
    auto numFrames = 0;

    SceneType sceneType = SceneType::Default;
    auto numBounces = 2;
    auto mrps = 0.0f;

    CoordinateSpace3f prevCamera;
    auto prevNumBounces = 0;
    auto prevSceneType = sceneType;

    auto gui = [&]()
    {
        const char* sceneTypes[] = {
            "Phonon",
            "Embree",
			"Radeon Rays"
        };

        ImGui::Combo("Scene Type", reinterpret_cast<int*>(&sceneType), sceneTypes, 3);
        ImGui::SliderInt("Bounces", &numBounces, 1, 4);
        ImGui::Text("%.2f MRPS", mrps);
    };

    auto display = [&]()
    {
        auto needsReset = (UIWindow::sCamera.origin != prevCamera.origin ||
                           UIWindow::sCamera.ahead != prevCamera.ahead ||
                           UIWindow::sCamera.up != prevCamera.up ||
                           numBounces != prevNumBounces ||
                           sceneType != prevSceneType);

        if (!sceneForType[sceneType])
        {
            if (sceneType == SceneType::Embree)
            {
                sceneForType[sceneType] = loadMesh(context, objFileName, mtlFileName, sceneType, nullptr,
                                                   nullptr, nullptr, nullptr, nullptr, embree);

                simulatorForType[sceneType] = ReflectionSimulatorFactory::create(sceneType, imageWidth * imageHeight,
                                                                                 1024, 0.1f, 0, 2, numThreads, 1, 1,
                                                                                 nullptr);
            }
			else if (sceneType == SceneType::RadeonRays)
			{
#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
                sceneForType[sceneType] = loadMesh(context, objFileName, mtlFileName, sceneType, nullptr,
												   nullptr, nullptr, nullptr, nullptr, nullptr, radeonRays);

				simulatorForType[sceneType] = ReflectionSimulatorFactory::create(sceneType, imageWidth * imageHeight,
																				 1024, 0.1f, 0, 2, numThreads, 1, 1,
																				 radeonRays);
#endif
			}
        }

        if (needsReset)
            numFrames = 0;

        listeners[0] = UIWindow::sCamera;

        auto& scene = *sceneForType[sceneType];
        auto& simulator = *simulatorForType[sceneType];

        Timer timer;
        timer.start();

        jobGraph.reset();
        simulator.simulate(scene, 2, sources, 1, listeners, directivities, imageWidth * imageHeight, numBounces, 0.1f, 0, 1.0f, image, jobGraph);
        threadPool.process(jobGraph);

        auto elapsedMilliseconds = timer.elapsedMilliseconds();

        mrps = static_cast<float>((2 * numBounces * imageWidth * imageHeight * 1e-3) / elapsedMilliseconds);

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
        prevSceneType = sceneType;
    };

    UIWindow window;
    window.run(gui, display, nullptr);
}