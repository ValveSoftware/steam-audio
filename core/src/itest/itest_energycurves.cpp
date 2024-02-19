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
#include <direct_simulator.h>
#include <energy_field_factory.h>
#include <opencl_energy_field.h>
#include <reflection_simulator_factory.h>
#include <thread_pool.h>
#include <fstream>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(energycurves)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto sceneType = SceneType::Default;

    auto scene = loadMesh(context, "boxroom.obj", "boxroom.mtl", sceneType, nullptr);
    auto phononScene = (sceneType == SceneType::Default) ? scene : loadMesh(context, "box.obj", "box.mtl", SceneType::Default, nullptr);
    auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(phononScene)->staticMeshes().front())->mesh();

    CoordinateSpace3f sources[2];
    sources[0] = CoordinateSpace3f{};
    sources[1] = CoordinateSpace3f{};

    CoordinateSpace3f listeners[1];

    Directivity directivities[2];
    directivities[0] = Directivity{};
    directivities[1] = Directivity{};

    std::string fileNamePrefix = "plotfile-box-";

    std::atomic<bool> stopSimulation{ false };
    ThreadPool threadPool(1);

	unique_ptr<EnergyField> energyFields[2];
	energyFields[0] = EnergyFieldFactory::create(sceneType, 1.0f, 0, nullptr);
	energyFields[1] = EnergyFieldFactory::create(sceneType, 1.0f, 0, nullptr);

	EnergyField* energyFieldPtrs[2];
	energyFieldPtrs[0] = energyFields[0].get();
	energyFieldPtrs[1] = energyFields[1].get();

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);
    };

    auto directivity = Directivity{ 0.0f, 0.0f };

    auto distance = 1.0f;
    auto directEnergy = 1.0f;
    auto sourceCentricEnergy = 1.0f;
    auto listenerCentricEnergy = 1.0f;
    auto directToSourceRatio = sqrtf(directEnergy / sourceCentricEnergy);
    auto directToListenerRatio = sqrtf(directEnergy / listenerCentricEnergy);
    auto sourceToListenerCorrection = sqrtf(sourceCentricEnergy / listenerCentricEnergy);

    auto oneSimulation = [&](Vector3f& sourcePos, CoordinateSpace3f& listener, float& _directEnergy, float& _sourceEnergy, float& _listenerEnergy)
    {
        listeners[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, listener.origin };

        sources[0] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, listener.origin };
        sources[1] = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, sourcePos };

        printf("Source: %.2f %.2f %.2f\n", sources[1].origin.elements[0], sources[1].origin.elements[1], sources[1].origin.elements[2]);
        printf("Listener: %.2f %.2f %.2f\n", sources[0].origin.elements[0], sources[0].origin.elements[1], sources[0].origin.elements[2]);

		auto simulator = ReflectionSimulatorFactory::create(sceneType, 8192, 1024, 1.0f, 0, 2, 1, 1, 1, nullptr);

        JobGraph jobGraph;
        simulator->simulate(*scene, 2, sources, 1, listeners, directivities, 8192, 64, 1.0f, 0, 1.0f, energyFieldPtrs, jobGraph);
        threadPool.process(jobGraph);

        auto channel = 0;
        auto band = 0;
        auto sourceCentricEnergyLocal = .0f;
        auto listenerCentricEnergyLocal = .0f;

        EnergyField* cpuEnergyFields[2];
        cpuEnergyFields[0] = energyFieldPtrs[0];
        cpuEnergyFields[1] = energyFieldPtrs[1];
        for (auto i = 0; i < cpuEnergyFields[0]->numBins(); ++i)
        {
            listenerCentricEnergyLocal += std::fabs((*cpuEnergyFields[0])[channel][band][i]);
            sourceCentricEnergyLocal += std::fabs((*cpuEnergyFields[1])[channel][band][i]);
        }

        DirectSoundPath directSoundPath;
        auto directSimulator = make_shared<DirectSimulator>(64);

        auto flags = (DirectSimulationFlags) (CalcDistanceAttenuation | CalcAirAbsorption | CalcDirectivity | CalcOcclusion);

        directSimulator->simulate(scene.get(), flags, sources[1], listener, DistanceAttenuationModel{}, AirAbsorptionModel{}, Directivity{}, OcclusionType::Raycast, 1.0f, 64, 1, directSoundPath);

        _directEnergy = directSoundPath.distanceAttenuation * directSoundPath.distanceAttenuation;
        _sourceEnergy = sourceCentricEnergyLocal;
        _listenerEnergy = listenerCentricEnergyLocal;
    };

    auto increment = .3f;
    auto minDistance = 1.0f;
    auto maxDistance = 30.0f;

	auto gui = [&]()
	{
		ImGui::SliderFloat("Increment", &increment, 0.1f, 2.0f);
		ImGui::SliderFloat("Min Distance", &minDistance, 0.5f, 2.0f);
		ImGui::SliderFloat("Max Distance", &maxDistance, 2.0f, 100.0f);
		ImGui::Spacing();

		if (ImGui::Button("Update Energy Plot"))
		{
			printf("\nUpdating Energy Plot\n");
			int numSamples = static_cast<int>((maxDistance - minDistance) / increment);

			static int fileIndex = 0;

			std::vector<float> distanceSamples(numSamples);
			std::vector<float> directEnergies(numSamples);
			std::vector<float> sourceEnergies(numSamples);
			std::vector<float> listenerEnergies(numSamples);
			std::vector<float> dToSRatios(numSamples);
			std::vector<float> dToLRatios(numSamples);
			std::vector<float> sToLCorrection(numSamples);

			for (auto i = 0U; i < distanceSamples.size(); ++i)
			{
				distanceSamples[i] = minDistance + i * increment;
			}

			Vector3f sourcePos(.0f, .0f, .0f);
			Vector3f direction = Vector3f::unitVector(Vector3f(1.0f, .0f, .0f));
			for (auto i = 0U; i < distanceSamples.size(); ++i)
			{
				Vector3f _listenerPos = sourcePos + distanceSamples[i] * direction;
				CoordinateSpace3f _listener{-Vector3f::kZAxis, Vector3f::kYAxis, _listenerPos};

				oneSimulation(sourcePos, _listener, directEnergies[i], sourceEnergies[i], listenerEnergies[i]);

				dToSRatios[i] = sqrtf(directEnergies[i] / sourceEnergies[i]);
				dToLRatios[i] = sqrtf(directEnergies[i] / listenerEnergies[i]);
				sToLCorrection[i] = sqrtf(sourceEnergies[i] / listenerEnergies[i]);
			}

			std::string fileName = fileNamePrefix + std::to_string(fileIndex++) + ".txt";
			std::ofstream outfile(fileName.c_str());
			for (auto i = 0U; i < distanceSamples.size(); ++i)
			{
				outfile << distanceSamples[i] << " ";
				outfile << directEnergies[i] << " ";
				outfile << sourceEnergies[i] << " ";
				outfile << listenerEnergies[i] << " ";
				outfile << dToSRatios[i] << " ";
				outfile << dToLRatios[i] << " ";
				outfile << sToLCorrection[i] << "\n";
			}
			outfile.close();
		}

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Energy Readout");
		ImGui::Text("Distance: %f", distance);
		ImGui::Text("Direct Energy: %f", directEnergy);
		ImGui::Text("Source Energy: %f", sourceCentricEnergy);
		ImGui::Text("Listener Energy: %f", listenerCentricEnergy);
		ImGui::Text("D-to-S Ratio: %f", directToSourceRatio);
		ImGui::Text("D-to-L Ratio: %f", directToListenerRatio);
		ImGui::Text("S-to-L Correction: %f", sourceToListenerCorrection);

		if (ImGui::Button("Update Energy Readout"))
		{
			printf("\nUpdating Energy Readout\n");
			auto _listener = UIWindow::sCamera;
			Vector3f _source(.0f, .0f, .0f);

			oneSimulation(_source, _listener, directEnergy, sourceCentricEnergy, listenerCentricEnergy);

			distance = (_listener.origin - _source).length();
			directToSourceRatio = sqrtf(directEnergy / sourceCentricEnergy);
			directToListenerRatio = sqrtf(directEnergy / listenerCentricEnergy);
			sourceToListenerCorrection = sqrtf(sourceCentricEnergy / listenerCentricEnergy);
		}
	};

	UIWindow window;
    window.run(gui, display, nullptr);
}