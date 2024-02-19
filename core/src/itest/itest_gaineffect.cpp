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

#include <gain_effect.h>
#include "helpers.h"
#include "ui_window.h"
using namespace ipl;

#include "itest.h"

ITEST(gaineffect)
{
    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    GainEffect gainEffect(audioSettings);

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer result(1, audioSettings.frameSize);

    GainEffectParams gainParams{};
    gainParams.gain = 1.0f;

	auto gui = [&]()
	{
		ImGui::SliderFloat("Gain", &gainParams.gain, 0.0f, 1.0f);
	};

    auto processAudio = [&](const AudioBuffer& in,
                              AudioBuffer& out)
    {
        AudioBuffer::downmix(in, mono);

        gainEffect.apply(gainParams, mono, result);

        for (auto i = 0; i < audioSettings.frameSize; ++i)
        {
            out[0][i] = result[0][i];
            out[1][i] = result[0][i];
        }
    };

    auto processTail = [&](AudioBuffer& out)
    {
        return gainEffect.tail(out);
    };

	UIWindow window;
	window.run(gui, nullptr, processAudio, processTail);
}
