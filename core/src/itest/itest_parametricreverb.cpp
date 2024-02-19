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
#include <energy_field_factory.h>
#include <reflection_simulator_factory.h>
#include <reverb_estimator.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(parametricreverb)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    SceneType sceneType = SceneType::Default;

    auto scene = loadMesh(context, "sponza.obj", "sponza.mtl", sceneType, nullptr);
    auto phononScene = (sceneType == SceneType::Default) ? scene : loadMesh(context, "sponza.obj", "sponza.mtl", SceneType::Default, nullptr);
    auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(phononScene)->staticMeshes().front())->mesh();

	auto simulator = ReflectionSimulatorFactory::create(sceneType, 8192, 1024, 2.0f, 0, 1, 1, 1, 1, nullptr);

    CoordinateSpace3f sources[1];
    sources[0] = CoordinateSpace3f{};

    CoordinateSpace3f listeners[1];

    AirAbsorptionModel airAbsorptions[1];
    airAbsorptions[0] = AirAbsorptionModel{};

    Directivity directivities[1];
    directivities[0] = Directivity{};

    std::atomic<bool> stopSimulation{ false };
    ThreadPool threadPool(1);

	unique_ptr<EnergyField> energyFields[1];
	energyFields[0] = EnergyFieldFactory::create(sceneType, 2.0f, 0, nullptr);

	Reverb reverbs[1];
	reverbs[0] = Reverb{};

	EnergyField* energyFieldPtrs[1];
	energyFieldPtrs[0] = energyFields[0].get();

    Array<float> edc(energyFields[0]->numBins());

    auto gui = [&]()
    {
        ImGui::PlotLines("EDC", edc.data(), static_cast<int>(edc.size(0)), 0, nullptr, -100.0f, 0.0f, ImVec2(512, 512));
        ImGui::Text("Reverb Time: (%.2f, %.2f, %.2f)", reverbs[0].reverbTimes[0], reverbs[0].reverbTimes[1], reverbs[0].reverbTimes[2]);
    };

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);
    };

    auto directivity = Directivity{};

    std::thread simulationThread([&]()
    {
        while (!stopSimulation)
        {
            listeners[0] = UIWindow::sCamera;
            sources[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, listeners[0].origin };

            JobGraph jobGraph;
            simulator->simulate(*scene, 1, sources, 1, listeners, directivities, 8192, 16, 2.0f, 0, 1.0f, energyFieldPtrs, jobGraph);
            threadPool.process(jobGraph);

            ReverbEstimator::estimate(*energyFieldPtrs[0], airAbsorptions[0], reverbs[0]);

            auto E = 0.0f;
            for (auto i = energyFields[0]->numBins() - 1; i >= 0; --i)
            {
                E += (*energyFieldPtrs[0])[0][1][i];
                edc[i] = E;
            }

            auto E0 = edc[0];
            for (auto i = 0; i < edc.size(0); ++i)
                edc[i] = 10.0f * log10f(edc[i] / E0);
        }
    });

    UIWindow window;
    window.run(gui, display, nullptr);

    stopSimulation = true;
    simulationThread.join();
}