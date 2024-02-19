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

#include <log.h>
#include <profiler.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkBinauralEffectWithInterpolation()
{
    const int kNumRuns = 100;
    const int kSamplingRate = 48000;
    const int kFrameSize = 1024;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, kFrameSize };

    IPLHRTF hrtf = nullptr;
    IPLHRTFSettings hrtfSettings{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    iplHRTFCreate(context, &dspParams, &hrtfSettings, &hrtf);

    IPLBinauralEffect effect = nullptr;
    double elapsedTime = .0;
    Timer timer;

    for (auto i = 0; i < kNumRuns; ++i)
    {
        timer.start();
        IPLBinauralEffectSettings effectSettings{ hrtf };
        iplBinauralEffectCreate(context, &dspParams, &effectSettings, &effect);

        elapsedTime += timer.elapsedMilliseconds();
        iplBinauralEffectRelease(&effect);
    }

    elapsedTime /= kNumRuns;
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    PrintOutput("Creation time per effect = %.5f ms\n", elapsedTime);
}

BENCHMARK(binauraleffect)
{
    PrintOutput("Running benchmark: Create Object-Based Binaural Effect...\n");
    BenchmarkBinauralEffectWithInterpolation();
    PrintOutput("\n");
}
