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

#include <panning_effect.h>
#include <virtual_surround_effect.h>
#include "ui_window.h"
#include "helpers.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(virtualsurround)
{
    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    HRTFSettings hrtfSettings{};
    HRTFDatabase hrtf(hrtfSettings, audioSettings.samplingRate, audioSettings.frameSize);

    auto speakerLayout = SpeakerLayout(SpeakerLayoutType::Stereo);

    PanningEffectSettings panningSettings{};
    panningSettings.speakerLayout = &speakerLayout;

    PanningEffect panningEffect(panningSettings);

    VirtualSurroundEffectSettings virtualSurroundSettings{};
    virtualSurroundSettings.speakerLayout = panningSettings.speakerLayout;
    virtualSurroundSettings.hrtf = &hrtf;

    VirtualSurroundEffect virtualSurroundEffect(audioSettings, virtualSurroundSettings);

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer surround(panningSettings.speakerLayout->numSpeakers, audioSettings.frameSize);

    Vector3f sourcePosition{ 0.0f, 0.0f, -1.0f };

    auto display = [&]()
    {
        UIWindow::drawPoint(sourcePosition, UIColor{ 1.0f, 0.0f, 0.0f });
    };

    auto processAudio = [&](const AudioBuffer& inBuffer, AudioBuffer& outBuffer)
    {
        AudioBuffer::downmix(inBuffer, mono);

        auto direction = UIWindow::sCamera.transformDirectionFromWorldToLocal(sourcePosition);

        PanningEffectParams panningParams{};
        panningParams.direction = &direction;

        panningEffect.apply(panningParams, mono, surround);

        VirtualSurroundEffectParams virtualSurroundParams{};
        virtualSurroundParams.hrtf = &hrtf;

        virtualSurroundEffect.apply(virtualSurroundParams, surround, outBuffer);
    };

    auto processTail = [&](AudioBuffer& outBuffer)
    {
        return virtualSurroundEffect.tail(outBuffer);
    };

	UIWindow window;
	window.run(nullptr, display, processAudio, processTail);
}