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
#include <simulation_data.h>
#include <simulation_manager.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(convolutioneffect)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    SceneType sceneType = SceneType::Default;
    IndirectEffectType indirectType = IndirectEffectType::Hybrid;
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

	SimulationManager simulationManager(false, true, false, sceneType, indirectType, 128, 1024, 1024, 1.0f, 1, 1, 1, 8, 1, 0, false, -Vector3f::kYAxis, 48000, 1024, openCL, radeonRays, tan);

    SharedSimulationData sharedData;
    sharedData.reflection.numRays = 1024;
    sharedData.reflection.numBounces = 16;
    sharedData.reflection.duration = 1.0f;
    sharedData.reflection.order = 1;
    sharedData.reflection.irradianceMinDistance = 1.0f;
    sharedData.reflection.reconstructionType = ReconstructionType::Linear;

    simulationManager.scene() = scene;

	auto source = ipl::make_shared<SimulationData>(true, false, sceneType, indirectType, 128, 1.0f, 1, 48000, 1024, openCL, tan);

    source->reflectionInputs.enabled = true;
    source->reflectionInputs.distanceAttenuationModel = DistanceAttenuationModel{};
    source->reflectionInputs.airAbsorptionModel = AirAbsorptionModel{};
    source->reflectionInputs.directivity = Directivity{};
    source->reflectionInputs.reverbScale[0] = 1.0f;
    source->reflectionInputs.reverbScale[1] = 1.0f;
    source->reflectionInputs.reverbScale[2] = 1.0f;
    source->reflectionInputs.transitionTime = 0.1f;
    source->reflectionInputs.overlapFraction = 0.25f;
    source->reflectionInputs.baked = false;

    simulationManager.addSource(source);
    simulationManager.commit();

    auto ambisonicsBufferOrder = 1;
    auto ambisonicsBufferChannels = (ambisonicsBufferOrder + 1) * (ambisonicsBufferOrder + 1);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    OverlapSaveConvolutionEffectSettings convolutionSettings{};
    convolutionSettings.numChannels = ambisonicsBufferChannels;
    convolutionSettings.irSize = static_cast<int>(ceilf(1.0f * audioSettings.samplingRate));

    HybridReverbEffectSettings hybrid_settings{};
    hybrid_settings.numChannels = convolutionSettings.numChannels;
    hybrid_settings.irSize = convolutionSettings.irSize;

    OverlapSaveConvolutionEffect convolutionEffect(audioSettings, convolutionSettings);
    HybridReverbEffect hybrid_effect(audioSettings, hybrid_settings);
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

	auto gui = [&]()
	{
		const char* reconstructionTypes[] = {
			"Gaussian",
			"Linear"
		};

		ImGui::SliderFloat3("Reverb Scale", source->reflectionInputs.reverbScale, 0.1f, 10.0f);
		ImGui::Combo("Reconstruction Type", reinterpret_cast<int*>(&sharedData.reflection.reconstructionType), reconstructionTypes, 2);
		ImGui::SliderFloat("IR Duration", &source->reflectionInputs.transitionTime, 0.1f, 2.0f);
	};

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);
    };

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
        else if (indirectType == IndirectEffectType::Hybrid)
        {
            HybridReverbEffectParams hybrid_params{};
            hybrid_params.fftIR = &source->reflectionOutputs.overlapSaveFIR;
            hybrid_params.numChannels = ambisonicsBufferChannels;
            hybrid_params.numSamples = hybrid_settings.irSize;
            hybrid_params.reverb = &source->reflectionOutputs.reverb;
            hybrid_params.eqCoeffs = source->reflectionOutputs.hybridEQ;
            hybrid_params.delay = source->reflectionOutputs.hybridDelay;

            hybrid_effect.apply(hybrid_params, mono, ambisonics);
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

    auto effectState = AudioEffectState::TailRemaining;
    auto processTail = [&](AudioBuffer& outBuffer)
    {
        if (indirectType == IndirectEffectType::TrueAudioNext)
        {
            outBuffer.makeSilent();
            return AudioEffectState::TailComplete;
        }
        else
        {
            if (effectState == AudioEffectState::TailRemaining)
            {
                // convolution effect still has tail
                effectState = convolutionEffect.tail(ambisonics);

                AmbisonicsBinauralEffectParams binauralParams{};
                binauralParams.hrtf = &hrtf;
                binauralParams.order = 1;

                return ambisonicsBinauralEffect.apply(binauralParams, ambisonics, outBuffer);
            }
            else
            {
                // convolution tail is finished, but might still need to flush the ambisonics binaural effect
                return ambisonicsBinauralEffect.tail(outBuffer);
            }
        }
    };

	UIWindow window;
	window.run(gui, display, processAudio, processTail);

    stopSimulation = true;
    simulationThread.join();
}