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

#include <delay_effect.h>
#include "helpers.h"
#include "ui_window.h"
using namespace ipl;

#include "itest.h"

ITEST(delayeffect)
{
    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    DelayEffectSettings delaySettings{};
    delaySettings.maxDelayInSamples = static_cast<int>(ceilf(2.0f * audioSettings.samplingRate));

    DelayEffect delayEffect(audioSettings, delaySettings);

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer result(1, audioSettings.frameSize);

    float delay = 0.0f;

	auto gui = [&]()
	{
		ImGui::SliderFloat("Delay", &delay, 0.01f, 1.0f);
	};

    auto processAudio = [&](const AudioBuffer& in, AudioBuffer& out)
    {
        AudioBuffer::downmix(in, mono);

        DelayEffectParams delayParams{};
        delayParams.delayInSamples = static_cast<int>(floorf(delay * audioSettings.samplingRate));

        delayEffect.apply(delayParams, mono, result);

        for (auto i = 0; i < audioSettings.frameSize; ++i)
        {
            out[0][i] = result[0][i];
            out[1][i] = result[0][i];
        }
    };

    auto processTail = [&](AudioBuffer& out)
    {
        auto effectState = delayEffect.tail(result);

        for (auto i = 0; i < audioSettings.frameSize; ++i)
        {
            out[0][i] = result[0][i];
            out[1][i] = result[0][i];
        }

        return effectState;
    };

	UIWindow window;
	window.run(gui, nullptr, processAudio, processTail);
}
