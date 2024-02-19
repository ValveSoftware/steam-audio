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
#include <ambisonics_encode_effect.h>
#include <ambisonics_binaural_effect.h>
#include <sh.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(ambisonicsbinauraleffect)
{
    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    HRTFSettings hrtfSettings{};
    HRTFDatabase hrtf(hrtfSettings, audioSettings.samplingRate, audioSettings.frameSize);

    auto order = 3;
    auto numChannels = SphericalHarmonics::numCoeffsForOrder(order);
    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer ambisonics(numChannels, audioSettings.frameSize);

    AmbisonicsEncodeEffectSettings encodeSettings{};
    encodeSettings.maxOrder = order;

    AmbisonicsEncodeEffect encodeEffect(encodeSettings);

    AmbisonicsBinauralEffectSettings binauralSettings{};
    binauralSettings.maxOrder = order;
    binauralSettings.hrtf = &hrtf;

    AmbisonicsBinauralEffect binauralEffect(audioSettings, binauralSettings);

    Vector3f source(1.0f, 0.0f, 0.0f);

    auto isolate = false;
    auto isolatedChannel = 1;
    auto flip = false;

	auto gui = [&]()
	{
		ImGui::Checkbox("Isolate", &isolate);
		ImGui::SliderInt("Isolated Channel", &isolatedChannel, 1, 3);
		ImGui::Checkbox("Flip", &flip);
	};

    auto display = [&]()
    {
        UIWindow::drawPoint(source, UIColor::kRed);
    };

    auto processAudio = [&](const AudioBuffer& in, AudioBuffer& out)
    {
        AudioBuffer::downmix(in, mono);

        auto direction = UIWindow::sCamera.transformDirectionFromWorldToLocal(source);

        AmbisonicsEncodeEffectParams encodeParams{};
        encodeParams.direction = &direction;
        encodeParams.order = order;

        encodeEffect.apply(encodeParams, mono, ambisonics);

        if (isolate)
        {
            auto other1 = (((isolatedChannel - 1) + 1) % 3) + 1;
            auto other2 = (((isolatedChannel - 1) + 2) % 3) + 1;

            memset(ambisonics[other1], 0, audioSettings.frameSize * sizeof(float));
            memset(ambisonics[other2], 0, audioSettings.frameSize * sizeof(float));
            memcpy(ambisonics[isolatedChannel], ambisonics[0], audioSettings.frameSize * sizeof(float));

            if (flip)
            {
                for (auto i = 0; i < ambisonics.numSamples(); ++i)
                    ambisonics[isolatedChannel][i] *= -1.0f;
            }
        }

        AmbisonicsBinauralEffectParams binauralParams{};
        binauralParams.hrtf = &hrtf;
        binauralParams.order = order;

        binauralEffect.apply(binauralParams, ambisonics, out);
    };

    auto processTail = [&](AudioBuffer& out)
    {
        return binauralEffect.tail(out);
    };

	UIWindow window;
	window.run(gui, display, processAudio, processTail);
}