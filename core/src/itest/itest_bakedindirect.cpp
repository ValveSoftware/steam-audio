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
#include <ambisonics_binaural_effect.h>
#include <baked_reflection_simulator.h>
#include <probe_generator.h>
#include <reflection_baker.h>
#include <reflection_simulator_factory.h>
#include <simulation_data.h>
#include <simulation_manager.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(bakedindirect)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    SceneType sceneType = SceneType::Default;
    IndirectEffectType indirectType = IndirectEffectType::Convolution;
	auto embree = (sceneType == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;

#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
	auto openCLDeviceList = (sceneType == SceneType::RadeonRays || indirectType == IndirectEffectType::TrueAudioNext) ? make_shared<OpenCLDeviceList>(OpenCLDeviceType::GPU, 8, 0.5f, (indirectType == IndirectEffectType::TrueAudioNext)) : nullptr;
	auto openCL = (sceneType == SceneType::RadeonRays || indirectType == IndirectEffectType::TrueAudioNext) ? make_shared<OpenCLDevice>((*openCLDeviceList)[0].platform, (*openCLDeviceList)[0].device, (*openCLDeviceList)[0].numConvolutionCUs, (*openCLDeviceList)[0].numIRUpdateCUs) : nullptr;
	auto radeonRays = (sceneType == SceneType::RadeonRays) ? ipl::make_shared<ipl::RadeonRaysDevice>(openCL) : nullptr;
#else
    auto openCL = nullptr;
    auto radeonRays = nullptr;
#endif

#if defined(IPL_USES_TRUEAUDIONEXT)
    auto tan = (indirectType == IndirectEffectType::TrueAudioNext) ? ipl::make_shared<TANDevice>(openCL->convolutionQueue(), openCL->irUpdateQueue(), 1024, 48000, 1, 1) : nullptr;
#else
    auto tan = ipl::make_shared<TANDevice>();
#endif

	auto scene = loadMesh(context, "sponza.obj", "sponza.mtl", sceneType, nullptr, nullptr, nullptr, nullptr, nullptr, embree, radeonRays);
	auto phononScene = (sceneType == SceneType::Default) ? scene : loadMesh(context, "sponza.obj", "sponza.mtl", SceneType::Default, nullptr);
	auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(phononScene)->staticMeshes().front())->mesh();

    Matrix4x4f localToWorldTransform;
    localToWorldTransform.identity();
    localToWorldTransform *= 80;

    ProbeArray probeArray;
    ProbeGenerator::generateProbes(*phononScene, localToWorldTransform, ProbeGenerationType::UniformFloor, 1.5f, 1.5f, probeArray);

    auto probeBatch = make_shared<ProbeBatch>();
    for (auto i = 0; i < probeArray.probes.size(0); ++i)
    {
        probeBatch->addProbe(probeArray.probes[i].influence);
    }

    probeBatch->commit();

	auto simulator = ReflectionSimulatorFactory::create(sceneType, 1024, 1024, 1.0f, 1, 1, 1, 4, 1, radeonRays);

    BakedDataIdentifier identifier;
    identifier.variation = BakedDataVariation::Reverb;
    identifier.type = BakedDataType::Reflections;
    identifier.endpointInfluence = Sphere{};

    auto progress = [](float progress, void* userData) { printf("\rBaking... %3.0f%% complete", 100.0f * progress); };
	ReflectionBaker::bake(*scene, *simulator, identifier, true, false, 1024, 16, 1.0f, 1.0f, 1, 1.0f, 4, 1, sceneType, openCL, *probeBatch, progress);
	printf("\n");

	SimulationManager simulationManager(false, true, false, sceneType, indirectType, 128, 1024, 1024, 1.0f, 1, 1, 1, 8, 1, 0, false, -Vector3f::kYAxis, 48000, 1024, openCL, radeonRays, tan);

    SharedSimulationData sharedData;
    sharedData.reflection.numRays = 1024;
    sharedData.reflection.numBounces = 16;
    sharedData.reflection.duration = 1.0f;
    sharedData.reflection.order = 1;
    sharedData.reflection.irradianceMinDistance = 1.0f;
    sharedData.reflection.reconstructionType = ReconstructionType::Gaussian;

    simulationManager.scene() = scene;
    simulationManager.addProbeBatch(probeBatch);

	auto source = ipl::make_shared<SimulationData>(true, false, sceneType, indirectType, 128, 1.0f, 1, 48000, 1024, openCL, tan);

    source->reflectionInputs.distanceAttenuationModel = DistanceAttenuationModel{};
    source->reflectionInputs.airAbsorptionModel = AirAbsorptionModel{};
    source->reflectionInputs.directivity = Directivity{};
    source->reflectionInputs.reverbScale[0] = 1.0f;
    source->reflectionInputs.reverbScale[1] = 1.0f;
    source->reflectionInputs.reverbScale[2] = 1.0f;
    source->reflectionInputs.transitionTime = 1.0f;
    source->reflectionInputs.overlapFraction = 0.25f;
    source->reflectionInputs.baked = true;
    source->reflectionInputs.bakedDataIdentifier = BakedDataIdentifier{BakedDataType::Reflections, BakedDataVariation::Reverb };

    simulationManager.addSource(source);
    simulationManager.commit();

    auto ambisonicsBufferOrder = 1;
    auto ambisonicsBufferChannels = (ambisonicsBufferOrder + 1) * (ambisonicsBufferOrder + 1);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 48000;
    audioSettings.frameSize = 1024;

    OverlapSaveConvolutionEffectSettings convolutionSettings{};
    convolutionSettings.numChannels = ambisonicsBufferChannels;
    convolutionSettings.irSize = static_cast<int>(ceilf(1.0f * audioSettings.samplingRate));

    OverlapSaveConvolutionEffect convolutionEffect(audioSettings, convolutionSettings);
#if defined(IPL_USES_TRUEAUDIONEXT)
    TANConvolutionEffect tanEffect;
	TANConvolutionMixer tanMixer;
#endif

    HRTFSettings hrtfSettings{};
    HRTFDatabase hrtf(hrtfSettings, audioSettings.samplingRate, audioSettings.frameSize);

    AmbisonicsBinauralEffectSettings binauralSettings{};
    binauralSettings.maxOrder = 1;
    binauralSettings.hrtf = &hrtf;

    AmbisonicsBinauralEffect ambisonicsBinauralEffect(audioSettings, binauralSettings);

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer ambisonics(ambisonicsBufferChannels, audioSettings.frameSize);

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);

        for (auto i = 0; i < probeArray.probes.size(0); ++i)
        {
            UIWindow::drawPoint((*probeBatch)[i].influence.center, UIColor::kBlack, 2.0f);
        }
    };

    auto numFramesProcessed = 0;

    std::atomic<bool> stopSimulation{ false };

    std::thread simulationThread([&]()
    {
        while (!stopSimulation)
        {
            sharedData.reflection.listener = UIWindow::sCamera;
            simulationManager.setSharedReflectionInputs(sharedData.reflection);

            source->reflectionInputs.source = CoordinateSpace3f{ -Vector3f::kZAxis, Vector3f::kYAxis, sharedData.reflection.listener.origin };

            simulationManager.simulateIndirect();
        }
    });

    auto processAudio = [&](const AudioBuffer& inBuffer, AudioBuffer& outBuffer)
    {
        AudioBuffer::downmix(inBuffer, mono);

#if defined(IPL_USES_TRUEAUDIONEXT)
		if (indirectType == IndirectEffectType::TrueAudioNext)
		{
            TANConvolutionEffectParams tanParams{};
            tanParams.tan = tan.get();
            tanParams.slot = source->reflectionOutputs.tanSlot;

            tanEffect.apply(tanParams, mono, tanMixer);

            TANConvolutionMixerParams tanMixerParams{};
            tanMixerParams.tan = tan.get();

            tanMixer.apply(tanMixerParams, ambisonics);
        }
		else
#endif
		{
            OverlapSaveConvolutionEffectParams convolutionParams{};
            convolutionParams.fftIR = &source->reflectionOutputs.overlapSaveFIR;
            convolutionParams.numChannels = ambisonicsBufferChannels;
            convolutionParams.numSamples = convolutionSettings.irSize;

            convolutionEffect.apply(convolutionParams, mono, ambisonics);
        }

        AmbisonicsBinauralEffectParams binauralParams{};
        binauralParams.hrtf = &hrtf;
        binauralParams.order = 1;

        ambisonicsBinauralEffect.apply(binauralParams, ambisonics, outBuffer);
    };

	UIWindow window;
	window.run(nullptr, display, processAudio);

    stopSimulation = true;
    simulationThread.join();
}
