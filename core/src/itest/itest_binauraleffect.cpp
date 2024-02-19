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

#include <binaural_effect.h>
#include "ui_window.h"
#include "helpers.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

struct HRTFContainer
{
	std::string shortName;
	shared_ptr<HRTFDatabase> hrtf;
	shared_ptr<HRTFDatabase> hrtfVis;
};

ITEST(binauraleffect)
{
    Vector3f sourcePosition{ 0.0f, 0.0f, -1.0f };

    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

	AudioSettings audioSettings{};
    audioSettings.samplingRate = 48000;
    audioSettings.frameSize = 1024;

	std::vector<HRTFContainer> hrtfs(4);

	hrtfs[0].shortName = "Default";
	hrtfs[0].hrtf = loadHRTF(context, 0.0f, HRTFNormType::None, audioSettings.samplingRate, audioSettings.frameSize);
	hrtfs[0].hrtfVis = loadHRTF(context, 0.0f, HRTFNormType::None, audioSettings.samplingRate, audioSettings.frameSize);

	hrtfs[1].shortName = "Default (RMS)";
	hrtfs[1].hrtf = loadHRTF(context, 0.0f, HRTFNormType::RMS, audioSettings.samplingRate, audioSettings.frameSize);
	hrtfs[1].hrtfVis = loadHRTF(context, 0.0f, HRTFNormType::RMS, audioSettings.samplingRate, audioSettings.frameSize);

	hrtfs[2].shortName = "D1";
	hrtfs[2].hrtf = loadHRTF(context, -7.75f, HRTFNormType::None, audioSettings.samplingRate, audioSettings.frameSize, "../../data/hrtf/sadie_d1.sofa");
	hrtfs[2].hrtfVis = loadHRTF(context, -7.75f, HRTFNormType::None, audioSettings.samplingRate, audioSettings.frameSize, "../../data/hrtf/sadie_d1.sofa");

	hrtfs[3].shortName = "D1 (RMS)";
	hrtfs[3].hrtf = loadHRTF(context, -7.75f, HRTFNormType::RMS, audioSettings.samplingRate, audioSettings.frameSize, "../../data/hrtf/sadie_d1.sofa");
	hrtfs[3].hrtfVis = loadHRTF(context, -7.75f, HRTFNormType::RMS, audioSettings.samplingRate, audioSettings.frameSize, "../../data/hrtf/sadie_d1.sofa");

	std::vector<const char*> hrtfShortNames;
	for (auto i = 0; i < hrtfs.size(); ++i)
		hrtfShortNames.push_back(hrtfs[i].shortName.c_str());

	BinauralEffectSettings effectSettings{};
	effectSettings.hrtf = hrtfs[0].hrtf.get();

    BinauralEffect binauralEffect(audioSettings, effectSettings);

    Array<complex_t> leftHrtf(hrtfs[0].hrtfVis->numSpectrumSamples());
    Array<complex_t> rightHrtf(hrtfs[0].hrtfVis->numSpectrumSamples());
	Array<float> leftHrir(hrtfs[0].hrtf->numSamples());
	Array<float> rightHrir(hrtfs[0].hrtf->numSamples());
	Array<float> plotData(hrtfs[0].hrtfVis->numSpectrumSamples());
	Array<float> plotHRIRData(hrtfs[0].hrtfVis->numSamples());
	FFT fft(hrtfs[0].hrtfVis->numSamples());

	Array<complex_t, 2> mInterpolatedHRTF;
	mInterpolatedHRTF.resize(2, hrtfs[0].hrtfVis->numSpectrumSamples());

	auto bilinear = false;
    auto spatialBlend = 1.0f;
    auto phaseType = HRTFPhaseType::None;
	auto channel = 0;
	auto dbGain = 0.0f;
	auto plothrtf = true;
	int selectedHRTF = 0;
	auto scale = 1;

	auto loudnessType = HRTFNormType::None;
	auto loudnessValue = 0.0f;
	auto loudnessFactor = 1.0f;
	bool recalculateReferenceLoudness = true;
	auto enableLoudnessNormalization = false;

	int prevSelectedHRTF = selectedHRTF;
	auto prevLoudnessType = loudnessType;
	float referenceLoudness = 0.0f;

	const char* phaseTypes[] = {
		"None",
		"Sphere ITD",
		"Full"
	};

	const char* volumeNormalizations[] = {
		"None",
		"RMS"
	};

	auto gui = [&]()
	{
		auto direction = UIWindow::sCamera.transformDirectionFromWorldToLocal(sourcePosition);
		loudnessFactor = 1.0f;

		if (prevSelectedHRTF != selectedHRTF)
		{
			recalculateReferenceLoudness = true;
			prevSelectedHRTF = selectedHRTF;
		}

		if (prevLoudnessType != loudnessType)
		{
			recalculateReferenceLoudness = true;
			prevLoudnessType = loudnessType;
		}

		if (recalculateReferenceLoudness)
		{
			std::vector< const complex_t* > hrtfData;
			hrtfData.push_back(leftHrtf.data());
			hrtfData.push_back(rightHrtf.data());
			hrtfs[selectedHRTF].hrtf->nearestHRTF(ipl::Vector3f(.0f, .0f, -1.0f), hrtfData.data(), 1.0f, HRTFPhaseType::None, nullptr);
			if (loudnessType == HRTFNormType::RMS)
			{
				referenceLoudness = Loudness::calculateRMSLoudness(hrtfs[selectedHRTF].hrtf->numSpectrumSamples(), audioSettings.samplingRate, hrtfData.data() );
			}
			else
			{
				referenceLoudness = 0.0f;
			}

			recalculateReferenceLoudness = false;
			printf("Reference Loudness is: %f\n", referenceLoudness);
		}

		if (bilinear)
		{
			complex_t* hrtfData[] = {leftHrtf.data(), rightHrtf.data()};
			hrtfs[selectedHRTF].hrtfVis->interpolatedHRTF(direction, hrtfData, spatialBlend, phaseType);
		}
		else
		{
			complex_t const* hrtfData[] = {nullptr, nullptr};
			hrtfs[selectedHRTF].hrtfVis->nearestHRTF(direction, hrtfData, spatialBlend, phaseType, mInterpolatedHRTF.data());

			if (spatialBlend < 1.0f)
			{
				hrtfData[0] = mInterpolatedHRTF[0];
				hrtfData[1] = mInterpolatedHRTF[1];
			}

			memcpy(leftHrtf.data(), hrtfData[0], leftHrtf.size(0) * sizeof(complex_t));
			memcpy(rightHrtf.data(), hrtfData[1], rightHrtf.size(0) * sizeof(complex_t));
		}

		{
			if (loudnessType == HRTFNormType::RMS)
			{
				std::vector< const complex_t* > hrtfData;
				hrtfData.push_back(leftHrtf.data());
				hrtfData.push_back(rightHrtf.data());
				loudnessValue = Loudness::calculateRMSLoudness(hrtfs[selectedHRTF].hrtf->numSpectrumSamples(), audioSettings.samplingRate, hrtfData.data());
			}

			loudnessFactor = Loudness::calculateGainScaling(loudnessValue, referenceLoudness+dbGain);
		}

		auto& hrtf = (channel == 0) ? leftHrtf : rightHrtf;

		for (auto i = 0; i < hrtf.size(0); ++i)
		{
			plotData[i] = log10f(abs(hrtf[i]));
		}
		fft.applyInverse(hrtf.data(), plotHRIRData.data());

		ImGui::Checkbox("Bilinear", &bilinear);
		ImGui::SliderFloat("Spatial Blend", &spatialBlend, 0.0f, 1.0f);
		ImGui::Combo("Phase Type", reinterpret_cast<int*>(&phaseType), phaseTypes, 3);
		ImGui::Checkbox("DC Correction", &HRTFDatabase::sEnableDCCorrectionForPhaseInterpolation);
		ImGui::Checkbox("Nyquist Correction", &HRTFDatabase::sEnableNyquistCorrectionForPhaseInterpolation);
		ImGui::Combo("HRTF", &selectedHRTF, hrtfShortNames.data(), static_cast<int>(hrtfs.size()));
		ImGui::SliderInt("Channel", &channel, 0, 1);
		ImGui::SliderFloat("dB Gain", &dbGain, -5.0, 5.0);
		ImGui::Checkbox("Normalize Loudness", &enableLoudnessNormalization);
		ImGui::Text("Loudness: %f dB (%f)", (loudnessValue-referenceLoudness), loudnessFactor);
		ImGui::Combo("Loudness Type", reinterpret_cast<int*>(&loudnessType), volumeNormalizations, 2);
		if (ImGui::Button(plothrtf ? "Plot HRIR" : "Plot HRTF"))
			plothrtf = !plothrtf;

		if (plothrtf)
			ImGui::PlotLines("HRTF", plotData.data(), static_cast<int>(plotData.size(0)), 0, nullptr, log10f(1e-3f), log10f(20.0f), ImVec2(512, 512));
		else
		{
			ImGui::SliderInt("Scale", &scale, 1, 4);
			ImGui::PlotLines("HRIR", plotHRIRData.data(), static_cast<int>(plotHRIRData.size(0)) / scale, 0, nullptr, -1.0f, 1.0f, ImVec2(512, 512));
		}
	};

    auto display = [&]()
    {
        UIWindow::drawPoint(sourcePosition, UIColor{ 1.0f, 0.0f, 0.0f });
    };

    auto processAudio = [&](const AudioBuffer& inBuffer, AudioBuffer& outBuffer)
    {
		auto direction = UIWindow::sCamera.transformDirectionFromWorldToLocal(sourcePosition);

		BinauralEffectParams params{};
		params.direction = &direction;
        params.interpolation = (bilinear) ? HRTFInterpolation::Bilinear : HRTFInterpolation::NearestNeighbor;
		params.spatialBlend = spatialBlend;
		params.phaseType = phaseType;
		params.hrtf = hrtfs[selectedHRTF].hrtf.get();

        binauralEffect.apply(params, inBuffer, outBuffer);

		if (enableLoudnessNormalization)
			outBuffer.scale(loudnessFactor);
    };

	auto processTail = [&](AudioBuffer& outBuffer)
	{
		return binauralEffect.tail(outBuffer);
	};

	UIWindow window;
	window.run(gui, display, processAudio, processTail);
}