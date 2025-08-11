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

#include <eq_effect.h>
#include "helpers.h"
#include "ui_window.h"
using namespace ipl;

#include "itest.h"

ITEST(eqeffect)
{
    IIRFilterer::sEnableSwitching = true;

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    EQEffect eqEffect(audioSettings);

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer result(1, audioSettings.frameSize);

    float eqGains[Bands::kNumBands];
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        eqGains[i] = 1.0f;
    }

	auto gui = [&]()
	{
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            char label[32] = {0};
            sprintf(label, "EQ band %d", i);

            ImGui::SliderFloat(label, &eqGains[i], 0.0f, 1.0f);
        }

        ImGui::Checkbox("8th Order", &IIR::sUseOrder8);
	};

    auto processAudio = [&](const AudioBuffer& in,
                              AudioBuffer& out)
    {
        AudioBuffer::downmix(in, mono);

        EQEffectParams eqParams{};
        eqParams.gains = eqGains;

        eqEffect.apply(eqParams, mono, result);

        for (auto i = 0; i < audioSettings.frameSize; ++i)
        {
            out[0][i] = result[0][i];
            out[1][i] = result[0][i];
        }
    };

    auto processTail = [&](AudioBuffer& out)
    {
        return eqEffect.tail(out);
    };

	UIWindow window;
	window.run(gui, nullptr, processAudio, processTail);
}
