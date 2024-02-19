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

#include <reverb_effect.h>
#include "ui_window.h"
#include "helpers.h"
using namespace ipl;

#include "itest.h"

ITEST(reverbeffect)
{
    AudioSettings audioSettings{};
    audioSettings.samplingRate = 48000;
    audioSettings.frameSize = 1024;

    ReverbEffect reverbEffect(audioSettings);

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer result(1, audioSettings.frameSize);

    Reverb reverb{};
    reverb.reverbTimes[0] = 2.0f;
    reverb.reverbTimes[1] = 1.5f;
    reverb.reverbTimes[2] = 1.0f;

    auto dry = false;
    auto wet = true;

	auto gui = [&]()
	{
		ImGui::SliderFloat3("Reverb Times", reverb.reverbTimes, 0.1f, 10.0f);
		ImGui::Checkbox("Dry", &dry);
		ImGui::Checkbox("Wet", &wet);
	};

    auto processAudio = [&](const AudioBuffer& inBuffer, AudioBuffer& outBuffer)
    {
        AudioBuffer::downmix(inBuffer, mono);

        ReverbEffectParams reverbParams{};
        reverbParams.reverb = &reverb;

        reverbEffect.apply(reverbParams, mono, result);

        outBuffer.makeSilent();

        if (dry)
        {
            for (auto i = 0; i < audioSettings.frameSize; ++i)
            {
                outBuffer[0][i] += mono[0][i];
                outBuffer[1][i] += mono[0][i];
            }
        }
        if (wet)
        {
            for (auto i = 0; i < audioSettings.frameSize; ++i)
            {
                outBuffer[0][i] += result[0][i];
                outBuffer[1][i] += result[0][i];
            }
        }
    };

    auto processTail = [&](AudioBuffer& outBuffer)
    {
        outBuffer.makeSilent();

        AudioEffectState effectState = reverbEffect.tail(result);

        if (wet)
        {
            for (auto i = 0; i < audioSettings.frameSize; ++i)
            {
                outBuffer[0][i] += result[0][i];
                outBuffer[1][i] += result[0][i];
            }
        }

        return effectState;
    };

	UIWindow window;
	window.run(gui, nullptr, processAudio, processTail);
}
