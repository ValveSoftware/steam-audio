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

#include <direct_effect.h>
#include "helpers.h"
#include "ui_window.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(directsoundeffect)
{
    const int kNumBands = Bands::kNumBands;

    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    DirectEffectSettings directSettings{};
    directSettings.numChannels = 1;

    DirectEffect effect(audioSettings, directSettings);

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer result(1, audioSettings.frameSize);

    DirectEffectParams directParams{};
    directParams.directPath.distanceAttenuation = 1.0f;
    for (auto i = 0; i < Bands::kNumBands; ++i)
        directParams.directPath.airAbsorption[i] = 1.0f;
    directParams.transmissionType = TransmissionType::FreqIndependent;

    bool applyEQ = false;

	auto gui = [&]()
	{
		ImGui::SliderFloat("Attenuation", &directParams.directPath.distanceAttenuation, 0.0f, 1.0f);
		ImGui::Checkbox("Apply EQ", &applyEQ);
		
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            char label[32] = {0};
            sprintf(label, "EQ band %d", i);

            ImGui::SliderFloat(label, &directParams.directPath.airAbsorption[i], 0.05f, 1.0f);
        }
	};

    auto processAudio = [&](const AudioBuffer& in, AudioBuffer& out)
    {
        AudioBuffer::downmix(in, mono);

        directParams.flags = DirectEffectFlags::ApplyDistanceAttenuation;
        if (applyEQ)
        {
            directParams.flags = (DirectEffectFlags) (directParams.flags | DirectEffectFlags::ApplyAirAbsorption);
        }

        effect.apply(directParams, mono, result);

        for (auto i = 0; i < audioSettings.frameSize; ++i)
        {
            out[0][i] = result[0][i];
            out[1][i] = result[0][i];
        }
    };

	UIWindow window;
	window.run(gui, nullptr, processAudio);
}
