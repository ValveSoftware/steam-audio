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
#include <impulse_response_factory.h>
#include <opencl_energy_field.h>
#include <opencl_impulse_response.h>
#include <reflection_simulator_factory.h>
#include <reconstructor_factory.h>
#include <thread_pool.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(impulseresponse)
{
	auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    SceneType sceneType = SceneType::Default;
	IndirectEffectType indirectType = IndirectEffectType::TrueAudioNext;
	auto embree = (sceneType == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;
#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS) && defined(IPL_USES_TRUEAUDIONEXT)
	auto openCLDeviceList = (sceneType == SceneType::RadeonRays || indirectType == IndirectEffectType::TrueAudioNext) ? make_shared<OpenCLDeviceList>(OpenCLDeviceType::GPU, 0, 0.0f, false) : nullptr;
	auto openCL = (sceneType == SceneType::RadeonRays || indirectType == IndirectEffectType::TrueAudioNext) ? make_shared<OpenCLDevice>((*openCLDeviceList)[0].platform, (*openCLDeviceList)[0].device, 0, 0) : nullptr;
	auto radeonRays = (sceneType == SceneType::RadeonRays) ? ipl::make_shared<ipl::RadeonRaysDevice>(openCL) : nullptr;
#else
    auto openCL = nullptr;
    auto radeonRays = nullptr;
#endif
	auto scene = loadMesh(context, "sponza.obj", "sponza.mtl", sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree, radeonRays);
	auto phononScene = (sceneType == SceneType::Default) ? scene : loadMesh(context, "sponza.obj", "sponza.mtl", SceneType::Default, nullptr);
	auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(phononScene)->staticMeshes().front())->mesh();

    auto samplingRate = 48000;
    auto frameSize = 1024;

	auto simulator = ReflectionSimulatorFactory::create(sceneType, 8192, 1024, 1.0f, 1, 1, 1, 1, 1, radeonRays);
	auto reconstructor = ReconstructorFactory::create(sceneType, indirectType, 1.0f, 1, samplingRate, radeonRays);

    CoordinateSpace3f sources[1];
    sources[0] = CoordinateSpace3f{};

    CoordinateSpace3f listeners[1];

    const float* distanceCurves[1];
    distanceCurves[0] = nullptr;

    AirAbsorptionModel airAbsorptions[1];
    airAbsorptions[0] = AirAbsorptionModel{};

    Directivity directivities[1];
    directivities[0] = Directivity{};

    std::atomic<bool> stopSimulation{ false };
    ThreadPool threadPool(1);

	unique_ptr<EnergyField> energyFields[1];
	energyFields[0] = EnergyFieldFactory::create(sceneType, 1.0f, 1, openCL);

	unique_ptr<ImpulseResponse> impulseResponses[1];
	impulseResponses[0] = ImpulseResponseFactory::create(indirectType, 1.0f, 1, samplingRate, openCL);

	EnergyField* energyFieldPtrs[1];
	ImpulseResponse* impulseResponsePtrs[1];
	energyFieldPtrs[0] = energyFields[0].get();
	impulseResponsePtrs[0] = impulseResponses[0].get();

	int displayChannelIndex = 0;
    auto reconstructionType = ReconstructionType::Gaussian;
    int sourceIndex = 1;

    Array<float> plotData(impulseResponses[0]->numSamples());
    memset(plotData.data(), 0, plotData.size(0) * sizeof(float));

    auto gui = [&]()
    {
        const char* reconstructionTypes[] = {
            "Gaussian",
            "Linear"
        };

        const auto& impulseResponse = *impulseResponsePtrs[0];

        ImGui::SliderInt("Channel", &displayChannelIndex, 0, 3);
        ImGui::Combo("Reconstruction", reinterpret_cast<int*>(&reconstructionType), reconstructionTypes, 2);
        ImGui::PlotLines("Impulse Response", plotData.data(), impulseResponse.numSamples(), 0, nullptr, -0.05f, 0.05f, ImVec2(512, 512));
    };

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);
    };

	UIWindow::sCamera = CoordinateSpace3f{ Vector3f{ -1.0f, .0f, .0f }, UIWindow::sCamera.up, Vector3f {.4f, .0f, -2.7f} };

    std::thread simulationThread([&]()
    {
        while (!stopSimulation)
        {
            listeners[0] = UIWindow::sCamera;
            sources[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, listeners[0].origin };

            JobGraph jobGraph;
            simulator->simulate(*scene, 1, sources, 1, listeners, directivities, 8192, 16, 1.0f, 1, 1.0f, energyFieldPtrs, jobGraph);
            threadPool.process(jobGraph);

#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS) && defined(IPL_USES_TRUEAUDIONEXT)
            if (sceneType == SceneType::RadeonRays && indirectType != IndirectEffectType::TrueAudioNext)
				static_cast<OpenCLEnergyField*>(energyFieldPtrs[0])->copyDeviceToHost();
#endif

            reconstructor->reconstruct(1, energyFieldPtrs, distanceCurves, airAbsorptions, impulseResponsePtrs, reconstructionType, 1.0f, 1);

#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS) && defined(IPL_USES_TRUEAUDIONEXT)
            if (sceneType == SceneType::RadeonRays && indirectType == IndirectEffectType::TrueAudioNext)
				static_cast<OpenCLImpulseResponse*>(impulseResponsePtrs[0])->copyDeviceToHost();
#endif

            memcpy(plotData.data(), (*impulseResponsePtrs[0])[displayChannelIndex], impulseResponsePtrs[0]->numSamples() * sizeof(float));
        }
    });

    UIWindow window;
    window.run(gui, display, nullptr);

    stopSimulation = true;
    simulationThread.join();
}