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
#include <reverb_effect.h>
using namespace ipl;

#include "phonon_perf.h"

BENCHMARK(reverb)
{
    PrintOutput("Running benchmark: Reverb...\n");
    PrintOutput("%9s %9s\n", "Time (ms)", "CPU Usage");

    const auto kNumRuns = 100000;
    const auto kSamplingRate = 48000;
    const auto kFrameSize = 1024;

    float in[kFrameSize];
    float out[kFrameSize];
    float* inData[] = { in };
    float* outData[] = { out };
    FillRandomData(in, kFrameSize);

    AudioBuffer inBuffer(1, kFrameSize, inData);
    AudioBuffer outBuffer(1, kFrameSize, outData);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = kSamplingRate;
    audioSettings.frameSize = kFrameSize;

    ReverbEffect reverbEffect(audioSettings);

    Reverb reverb;
    reverb.reverbTimes[0] = 2.0f;
    reverb.reverbTimes[1] = 1.5f;
    reverb.reverbTimes[2] = 1.0f;

    Timer timer{};
    timer.start();

    for (auto i = 0; i < kNumRuns; ++i)
    {
        ReverbEffectParams reverbParams{};
        reverbParams.reverb = &reverb;

        reverbEffect.apply(reverbParams, inBuffer, outBuffer);
    }

    auto timePerRun = timer.elapsedMilliseconds() / kNumRuns;

    auto frameTime = static_cast<double>(kFrameSize * 1000) / static_cast<double>(kSamplingRate);
    auto cpuUsage = (timePerRun / frameTime) * 100.0;

    PrintOutput("%8.1f %8.1f%%\n", timePerRun * 1000.0, cpuUsage);
    PrintOutput("\n");
}
