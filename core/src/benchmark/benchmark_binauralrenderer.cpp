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

void BenchmarkHRTFCreateInitialization(int frameSize, IPLHRTFSettings hrtfParams)
{
    const int kNumRuns = 1;
    const int kSamplingRate = 48000;
    const int kFrameSize = frameSize;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, kFrameSize };

    double timePerRun, totalTime = .0;
    IPLHRTF binauralRenderer = nullptr;

    Timer timer;

    for (auto i = 0; i < kNumRuns; ++i)
    {
        timer.start();
        iplHRTFCreate(context, &dspParams, &hrtfParams, &binauralRenderer);
        totalTime += timer.elapsedMilliseconds();

        iplHRTFRelease(&binauralRenderer);
    }

    timePerRun = totalTime / kNumRuns;
    PrintOutput("%-12d  %7.2f %12s %12s\n", frameSize, timePerRun, hrtfParams.normType == IPL_HRTFNORMTYPE_NONE ? "None" : "RMS", hrtfParams.type == IPL_HRTFTYPE_DEFAULT ? "Default" : "SOFA");

    iplContextRelease(&context);
}

BENCHMARK(hrtf)
{
    PrintOutput("Running benchmark: Creating HRTF...\n");
    PrintOutput("%-13s %s  %s  %s", "Frame Size", "Time (msec)", "Norm Type", "HRTF Type\n");

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkHRTFCreateInitialization(256, hrtfParams);
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
        BenchmarkHRTFCreateInitialization(2048, hrtfParams);
        BenchmarkHRTFCreateInitialization(4096, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, "../../data/hrtf/sadie_d1.sofa", nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkHRTFCreateInitialization(256, hrtfParams);
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
        BenchmarkHRTFCreateInitialization(2048, hrtfParams);
        BenchmarkHRTFCreateInitialization(4096, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, "../../data/hrtf/sadie_h12.sofa", nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkHRTFCreateInitialization(256, hrtfParams);
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
        BenchmarkHRTFCreateInitialization(2048, hrtfParams);
        BenchmarkHRTFCreateInitialization(4096, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 0.5f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 0.5f, IPL_HRTFNORMTYPE_RMS };
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, "../../data/hrtf/sadie_d1.sofa", nullptr, 0, 0.5f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, "../../data/hrtf/sadie_d1.sofa", nullptr, 0, 0.5f, IPL_HRTFNORMTYPE_RMS };
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, "../../data/hrtf/sadie_h12.sofa", nullptr, 0, 0.5f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, "../../data/hrtf/sadie_h12.sofa", nullptr, 0, 0.5f, IPL_HRTFNORMTYPE_RMS };
        BenchmarkHRTFCreateInitialization(512, hrtfParams);
        BenchmarkHRTFCreateInitialization(1024, hrtfParams);
    }

    PrintOutput("\n");
}
