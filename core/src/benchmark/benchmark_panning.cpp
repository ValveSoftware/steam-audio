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

#include <profiler.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkPanningForSpeakerLayout(IPLSpeakerLayoutType channelLayout, int numChannels)
{
    const int kNumRuns = 1000;
    const int kSamplingRate = 48000;
    const int kFrameSize = 1024;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, kFrameSize };

    float inData[kFrameSize] = { 0 };
    float outData[8 * kFrameSize] = { 0 };
    FillRandomData(inData, kFrameSize);

    IPLPanningEffect effect = nullptr;
    IPLPanningEffectSettings effectSettings{ IPLSpeakerLayout{channelLayout} };
    iplPanningEffectCreate(context, &dspParams, &effectSettings, &effect);

    IPLAudioBuffer inBuffer;
    IPLAudioBuffer outBuffer;
    iplAudioBufferAllocate(context, 1, kFrameSize, &inBuffer);
    iplAudioBufferAllocate(context, numChannels, kFrameSize, &outBuffer);

    iplAudioBufferDeinterleave(context, inData, &inBuffer);

    IPLVector3 direction = { 1.0f, 0.0f, 0.0f };

    double timePerRun;

    Timer timer;
    timer.start();

    for (auto i = 0; i < kNumRuns; ++i)
    {
        IPLPanningEffectParams params{ direction };
        iplPanningEffectApply(effect, &params, &inBuffer, &outBuffer);
    }

    timePerRun = timer.elapsedSeconds() / kNumRuns;

    iplAudioBufferFree(context, &inBuffer);
    iplAudioBufferFree(context, &outBuffer);
    iplPanningEffectRelease(&effect);
    iplContextRelease(&context);

    auto frameTime = static_cast<double>(kFrameSize) / static_cast<double>(kSamplingRate);
    auto cpuUsage = (timePerRun / frameTime) * 100.0;
    auto numSources = static_cast<int>(floor(frameTime / timePerRun));

    const char* channelLayoutName[] = { "Mono", "Stereo", "Quadraphonic", "5.1", "7.1" };

    PrintOutput("%-20s %8.1f%% %13d\n", channelLayoutName[channelLayout], cpuUsage, numSources);
}

BENCHMARK(panning)
{
    PrintOutput("Running benchmark: Panning...\n");
    PrintOutput("%-20s %9s %13s\n", "Speaker Layout", "CPU Usage", "Max Sources");

    BenchmarkPanningForSpeakerLayout(IPL_SPEAKERLAYOUTTYPE_STEREO, 2);
    BenchmarkPanningForSpeakerLayout(IPL_SPEAKERLAYOUTTYPE_QUADRAPHONIC, 4);
    BenchmarkPanningForSpeakerLayout(IPL_SPEAKERLAYOUTTYPE_SURROUND_5_1, 6);
    BenchmarkPanningForSpeakerLayout(IPL_SPEAKERLAYOUTTYPE_SURROUND_7_1, 8);

    PrintOutput("\n");
}
