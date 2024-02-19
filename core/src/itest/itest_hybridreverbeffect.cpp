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

#include <stdint.h>

#include <phonon.h>

#include "ui_window.h"
#include <audio_buffer.h>
using namespace ipl;

#include "itest.h"


ITEST(hybridreverbeffect)
{
    IPLContextSettings contextSettings{};
    contextSettings.version = STEAMAUDIO_VERSION;
    contextSettings.simdLevel = IPL_SIMDLEVEL_AVX2;

    IPLContext context = nullptr;
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings audioSettings{};
    audioSettings.samplingRate = 48000;
    audioSettings.frameSize = 1024;

    IPLHRTFSettings hrtfSettings{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };

    IPLHRTF hrtf = nullptr;
    iplHRTFCreate(context, &audioSettings, &hrtfSettings, &hrtf);

    IPLReflectionEffectSettings reflectionEffectSettings{};
    reflectionEffectSettings.type = IPL_REFLECTIONEFFECTTYPE_HYBRID;
    reflectionEffectSettings.irSize = 48000;
    reflectionEffectSettings.numChannels = 4;

    IPLReflectionEffect reflectionEffect = nullptr;
    iplReflectionEffectCreate(context, &audioSettings, &reflectionEffectSettings, &reflectionEffect);

    IPLAmbisonicsDecodeEffectSettings ambisonicsDecodeEffectSettings{};
    ambisonicsDecodeEffectSettings.maxOrder = 1;
    ambisonicsDecodeEffectSettings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
    ambisonicsDecodeEffectSettings.hrtf = hrtf;

    IPLAmbisonicsDecodeEffect ambisonicsDecodeEffect = nullptr;
    iplAmbisonicsDecodeEffectCreate(context, &audioSettings, &ambisonicsDecodeEffectSettings, &ambisonicsDecodeEffect);

    IPLAudioBuffer monoBuffer{};
    IPLAudioBuffer indirectBuffer{};
    iplAudioBufferAllocate(context, 1, audioSettings.frameSize, &monoBuffer);
    iplAudioBufferAllocate(context, reflectionEffectSettings.numChannels, audioSettings.frameSize, &indirectBuffer);

    auto gui = [&]()
    {};

    auto display = [&]()
    {};

    auto processAudio = [&](const AudioBuffer& inBufferRaw, AudioBuffer& outBufferRaw)
    {
        IPLAudioBuffer inBuffer{};
        inBuffer.numChannels = inBufferRaw.numChannels();
        inBuffer.numSamples = inBufferRaw.numSamples();
        inBuffer.data = const_cast<float**>(inBufferRaw.data());

        IPLAudioBuffer outBuffer{};
        outBuffer.numChannels = outBufferRaw.numChannels();
        outBuffer.numSamples = outBufferRaw.numSamples();
        outBuffer.data = const_cast<float**>(outBufferRaw.data());

        iplAudioBufferDownmix(context, &inBuffer, &monoBuffer);

        IPLReflectionEffectParams reflectionEffectParams{};
        reflectionEffectParams.type = reflectionEffectSettings.type;
        reflectionEffectParams.irSize = reflectionEffectSettings.irSize;
        reflectionEffectParams.numChannels = reflectionEffectSettings.numChannels;
        reflectionEffectParams.ir = nullptr;
        reflectionEffectParams.reverbTimes[0] = 2.0f;
        reflectionEffectParams.reverbTimes[1] = 1.5f;
        reflectionEffectParams.reverbTimes[2] = 1.0f;
        reflectionEffectParams.eq[0] = 1.0f / 16.0f;
        reflectionEffectParams.eq[1] = 1.0f / 16.0f;
        reflectionEffectParams.eq[2] = 1.0f / 16.0f;
        reflectionEffectParams.delay = 0;

        iplReflectionEffectApply(reflectionEffect, &reflectionEffectParams, &monoBuffer, &indirectBuffer, nullptr);

        IPLAmbisonicsDecodeEffectParams ambisonicsDecodeEffectParams{};
        ambisonicsDecodeEffectParams.order = 1;
        ambisonicsDecodeEffectParams.hrtf = hrtf;
        ambisonicsDecodeEffectParams.orientation.origin = IPLVector3{0.0f, 0.0f, 0.0f};
        ambisonicsDecodeEffectParams.orientation.right = IPLVector3{1.0f, 0.0f, 0.0f};
        ambisonicsDecodeEffectParams.orientation.up = IPLVector3{0.0f, 1.0f, 0.0f};
        ambisonicsDecodeEffectParams.orientation.ahead = IPLVector3{0.0f, 0.0f, -1.0f};
        ambisonicsDecodeEffectParams.binaural = IPL_TRUE;

        iplAmbisonicsDecodeEffectApply(ambisonicsDecodeEffect, &ambisonicsDecodeEffectParams, &indirectBuffer, &outBuffer);
    };

    UIWindow window;
    window.run(gui, display, processAudio);

    iplAudioBufferFree(context, &indirectBuffer);
    iplAudioBufferFree(context, &monoBuffer);
    iplAmbisonicsDecodeEffectRelease(&ambisonicsDecodeEffect);
    iplReflectionEffectRelease(&reflectionEffect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);
}