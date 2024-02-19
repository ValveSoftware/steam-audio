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
#include <opencl_energy_field.h>
#include <reflection_simulator_factory.h>
#include <thread_pool.h>
#include <fstream>
#include <iomanip>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

void saveEnergyField(const EnergyField& energyField, int channel, int band, SceneType scene)
{
    std::string fileName = "ef-";
    if (scene == SceneType::Default)
        fileName += "phonon-";
    else if (scene == SceneType::Embree)
        fileName += "embree-";
    else if (scene == SceneType::RadeonRays)
        fileName += "radeonrays-";

    fileName += std::to_string(channel) + "-";
    fileName += std::to_string(band) + ".txt";

    std::ofstream myfile(fileName);
    for (int i = 0; i < energyField.numBins(); ++i)
    {
        myfile << std::scientific << energyField[channel][band][i] << std::endl;
    }

    myfile.close();
}

ITEST(energyfield)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    SceneType sceneType = SceneType::Default;
	auto embree = (sceneType == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;
#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
    auto openCLDeviceList = (sceneType == SceneType::RadeonRays) ? make_shared<OpenCLDeviceList>(OpenCLDeviceType::GPU, 0, 0.0f, false) : nullptr;
	auto openCL = (sceneType == SceneType::RadeonRays) ? make_shared<OpenCLDevice>((*openCLDeviceList)[0].platform, (*openCLDeviceList)[0].device, 0, 0) : nullptr;
	auto radeonRays = (sceneType == SceneType::RadeonRays) ? ipl::make_shared<ipl::RadeonRaysDevice>(openCL) : nullptr;
#else
    auto openCL = nullptr;
    auto radeonRays = nullptr;
#endif

	auto scene = loadMesh(context, "sponza.obj", "sponza.mtl", sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree, radeonRays);
	auto phononScene = (sceneType == SceneType::Default) ? scene : loadMesh(context, "sponza.obj", "sponza.mtl", SceneType::Default, nullptr);
	auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(phononScene)->staticMeshes().front())->mesh();

	auto simulator = ReflectionSimulatorFactory::create(sceneType, 8192, 1024, 1.0f, 1, 1, 1, 1, 1, radeonRays);

    CoordinateSpace3f sources[1];
    sources[0] = CoordinateSpace3f{};

    CoordinateSpace3f listeners[1];

    Directivity directivities[1];
    directivities[0] = Directivity{};

    std::atomic<bool> stopSimulation{ false };
    ThreadPool threadPool(1);

	unique_ptr<EnergyField> energyFields[1];
	energyFields[0] = EnergyFieldFactory::create(sceneType, 1.0f, 1, openCL);

	EnergyField* energyFieldPtrs[1];
	energyFieldPtrs[0] = energyFields[0].get();

	Array<float> plotData(energyFields[0]->numBins());
	memset(plotData.data(), 0, plotData.size(0) * sizeof(float));

    int displayChannelIndex = 0;
    bool saveEnergyFieldNextFrame = false;

    auto gui = [&]()
    {
        const auto& energyField = *energyFields[0];

        ImGui::SliderInt("Channel", &displayChannelIndex, 0, 3);
        ImGui::PlotLines("Energy Field", plotData.data(), energyField.numBins(), 0, nullptr, -0.001f, 0.001f, ImVec2(512, 512));
        saveEnergyFieldNextFrame = ImGui::Button("Save Energy Field");
    };

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);
    };

    UIWindow::sCamera = CoordinateSpace3f{ Vector3f{ -1.0f, .0f, .0f }, UIWindow::sCamera.up, Vector3f{ .4f, .0f, -2.7f } };

    std::thread simulationThread([&]()
    {
        while (!stopSimulation)
        {
            listeners[0] = UIWindow::sCamera;
            sources[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, listeners[0].origin };

            JobGraph jobGraph;
			simulator->simulate(*scene, 1, sources, 1, listeners, directivities, 8192, 16, 1.0f, 1, 1.0f, energyFieldPtrs, jobGraph);
			threadPool.process(jobGraph);

#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
            if (sceneType == SceneType::RadeonRays)
				static_cast<OpenCLEnergyField*>(energyFieldPtrs[0])->copyDeviceToHost();
#endif

			memcpy(plotData.data(), (*energyFields[0])[displayChannelIndex][0], energyFields[0]->numBins() * sizeof(float));

            auto channel = displayChannelIndex;
            auto band = 0;

            //float prevFactor = .0f;
            //float nextFactor = 1.0f;
            //if (simulationSettings.sceneType == SceneType::Embree || simulationSettings.sceneType == SceneType::RadeonRays)
            //{
            //    prevFactor = .9f;
            //    nextFactor = .1f;
            //}

            //auto cpuEnergyField = reinterpret_cast<CpuEnergyField*>(simulationData->mEnergyFieldMarshallers[0]->mCPUEnergyField.get());
            //for (auto i = 0; i < energyFieldData.size(); ++i)
            //{
            //    energyFieldData[i] = energyFieldData[i] * prevFactor + nextFactor * cpuEnergyField->echogram(channel).histogram(band).energy(i);
            //}

            if (saveEnergyFieldNextFrame)
            {
				saveEnergyField(*energyFieldPtrs[0], channel, band, sceneType);
				saveEnergyFieldNextFrame = false;
            }
        }
    });

    UIWindow window;
    window.run(gui, display, nullptr);

    stopSimulation = true;
    simulationThread.join();
}