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
#include "ui_window.h"
#include "helpers.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(panningeffect)
{
    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto samplingRate = 48000;
    auto frameSize = 1024;

    AudioBuffer mono(1, frameSize);

    auto speakerLayout = SpeakerLayout(SpeakerLayoutType::Stereo);

    PanningEffectSettings panningSettings{};
    panningSettings.speakerLayout = &speakerLayout;

    PanningEffect effect(panningSettings);

    Vector3f source(1.0f, 0.0f, 0.0f);

    auto display = [&]()
    {
        UIWindow::drawPoint(source, UIColor::kRed);
    };

    auto processAudio = [&](const AudioBuffer& inBuffer, AudioBuffer& outBuffer)
    {
        AudioBuffer::downmix(inBuffer, mono);

        auto direction = UIWindow::sCamera.transformDirectionFromWorldToLocal(source);

        PanningEffectParams params{};
        params.direction = &direction;

        effect.apply(params, inBuffer, outBuffer);
    };

    auto processTail = [&](AudioBuffer& outBuffer)
    {
        return effect.tail(outBuffer);
    };

	UIWindow window;
	window.run(nullptr, display, processAudio, processTail);
}