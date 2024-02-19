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

#include <ambisonics_binaural_effect.h>
#include <ambisonics_rotate_effect.h>
#include "helpers.h"
#include "ui_window.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(ambisonicsrotateeffect)
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

    AmbisonicsRotateEffectSettings rotateSettings{};
    rotateSettings.maxOrder = order;

    AmbisonicsRotateEffect rotateEffect(audioSettings, rotateSettings);

    AmbisonicsBinauralEffectSettings binauralSettings{};
    binauralSettings.maxOrder = order;
    binauralSettings.hrtf = &hrtf;

    AmbisonicsBinauralEffect binauralEffect(audioSettings, binauralSettings);

    auto processAudio = [&](const AudioBuffer& in, AudioBuffer& out)
    {
        AudioBuffer::downmix(in, mono);

        memcpy(ambisonics[0], mono[0], audioSettings.frameSize * sizeof(float));
        memcpy(ambisonics[1], mono[0], audioSettings.frameSize * sizeof(float));
        memset(ambisonics[2], 0, audioSettings.frameSize * sizeof(float));
        memset(ambisonics[3], 0, audioSettings.frameSize * sizeof(float));

        AmbisonicsRotateEffectParams rotateParams{};
        rotateParams.orientation = &UIWindow::sCamera;
        rotateParams.order = order;

        rotateEffect.apply(rotateParams, ambisonics, ambisonics);

        AmbisonicsBinauralEffectParams binauralParams{};
        binauralParams.hrtf = &hrtf;
        binauralParams.order = order;

        binauralEffect.apply(binauralParams, ambisonics, out);
    };

	UIWindow window;
	window.run(nullptr, nullptr, processAudio);
}
