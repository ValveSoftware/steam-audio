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

void BenchmarkVirtualSurroundForChannelLayout(IPLSpeakerLayoutType channelLayout, int numChannels)
{
    const int kNumRuns = 1000;
    const int kSamplingRate = 48000;
    const int kFrameSize = 1024;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, kFrameSize };

    IPLHRTF binauralRenderer = nullptr;
    IPLHRTFSettings hrtfSettings{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    iplHRTFCreate(context, &dspParams, &hrtfSettings, &binauralRenderer);

    float inData[8 * kFrameSize] = { 0 };
    float outData[2 * kFrameSize] = { 0 };
    FillRandomData(inData, 8 * kFrameSize);

    IPLAudioBuffer inBuffer;
    IPLAudioBuffer outBuffer;
    iplAudioBufferAllocate(context, numChannels, kFrameSize, &inBuffer);
    iplAudioBufferAllocate(context, 2, kFrameSize, &outBuffer);

    iplAudioBufferDeinterleave(context, inData, &inBuffer);

    IPLVirtualSurroundEffect effect = nullptr;
    IPLVirtualSurroundEffectSettings effectSettings{ IPLSpeakerLayout{channelLayout}, binauralRenderer };
    iplVirtualSurroundEffectCreate(context, &dspParams, &effectSettings, &effect);

    double timePerRun;

    ipl::Timer timer;
    timer.start();

    for (auto i = 0; i < kNumRuns; ++i)
    {
        IPLVirtualSurroundEffectParams params{ binauralRenderer };
        iplVirtualSurroundEffectApply(effect, &params, &inBuffer, &outBuffer);
    }

    timePerRun = timer.elapsedSeconds() / kNumRuns;

    iplAudioBufferFree(context, &inBuffer);
    iplAudioBufferFree(context, &outBuffer);
    iplVirtualSurroundEffectRelease(&effect);
    iplHRTFRelease(&binauralRenderer);
    iplContextRelease(&context);

    auto frameTime = static_cast<double>(kFrameSize) / static_cast<double>(kSamplingRate);
    auto cpuUsage = (timePerRun / frameTime) * 100.0;

    const char* channelLayoutName[] = { "Mono", "Stereo", "Quadraphonic", "5.1", "7.1" };

    PrintOutput("%-20s %8.1f%%\n", channelLayoutName[channelLayout], cpuUsage);
}

BENCHMARK(virtualsurround)
{
    PrintOutput("Running benchmark: Virtual Surround...\n");
    PrintOutput("%-20s %9s\n", "Speaker Layout", "CPU Usage");
    BenchmarkVirtualSurroundForChannelLayout(IPL_SPEAKERLAYOUTTYPE_STEREO, 2);
    BenchmarkVirtualSurroundForChannelLayout(IPL_SPEAKERLAYOUTTYPE_QUADRAPHONIC, 4);
    BenchmarkVirtualSurroundForChannelLayout(IPL_SPEAKERLAYOUTTYPE_SURROUND_5_1, 6);
    BenchmarkVirtualSurroundForChannelLayout(IPL_SPEAKERLAYOUTTYPE_SURROUND_7_1, 8);
    PrintOutput("\n");
}
