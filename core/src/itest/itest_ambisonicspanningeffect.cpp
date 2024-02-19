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

#include <ambisonics_encode_effect.h>
#include <ambisonics_panning_effect.h>
#include <sh.h>
#include "ui_window.h"
#include "helpers.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(ambisonicspanningeffect)
{
    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    auto order = 3;
    auto numChannels = SphericalHarmonics::numCoeffsForOrder(order);
    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer ambisonics(numChannels, audioSettings.frameSize);

    AmbisonicsEncodeEffectSettings encodeSettings{};
    encodeSettings.maxOrder = order;

    AmbisonicsEncodeEffect encodeEffect(encodeSettings);

    auto speakerLayout = SpeakerLayout(SpeakerLayoutType::Stereo);

    AmbisonicsPanningEffectSettings panningSettings{};
    panningSettings.speakerLayout = &speakerLayout;
    panningSettings.maxOrder = order;

    AmbisonicsPanningEffect panningEffect(audioSettings, panningSettings);

    Vector3f source(1.0f, 0.0f, 0.0f);

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

        AmbisonicsPanningEffectParams panningParams{};
        panningParams.order = order;

        panningEffect.apply(panningParams, ambisonics, out);
    };

	UIWindow window;
	window.run(nullptr, display, processAudio);
}