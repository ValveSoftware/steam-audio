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
#include <array.h>
#include <sh.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkAmbisonicsBinauralForOrder(int order, int frameSize)
{
    const int kNumRuns = 1000;
    const int kSamplingRate = 48000;
    int numChannels = (order + 1) * (order + 1);

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, frameSize };

    IPLHRTF hrtf = nullptr;
    IPLHRTFSettings hrtfSettings{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    iplHRTFCreate(context, &dspParams, &hrtfSettings, &hrtf);

    ipl::Array<float, 2> inData(numChannels, frameSize);
    FillRandomData(inData.flatData(), inData.totalSize());

    ipl::Array<float, 2> outData(2, frameSize);
    outData.zero();

    IPLAmbisonicsBinauralEffect effect = nullptr;
    IPLAmbisonicsBinauralEffectSettings effectSettings{ hrtf, order };
    iplAmbisonicsBinauralEffectCreate(context, &dspParams, &effectSettings, &effect);

    IPLAudioBuffer inBuffer{ numChannels, frameSize, inData.data() };
    IPLAudioBuffer outBuffer{ 2, frameSize, outData.data() };

    double timePerRun;

    Timer timer;
    timer.start();

    for (auto i = 0; i < kNumRuns; ++i)
    {
        IPLAmbisonicsBinauralEffectParams params{ hrtf, order };
        iplAmbisonicsBinauralEffectApply(effect, &params, &inBuffer, &outBuffer);
    }

    timePerRun = timer.elapsedSeconds() / kNumRuns;
    auto frameTime = static_cast<double>(frameSize) / static_cast<double>(kSamplingRate);
    auto numSources = static_cast<int>(floor(frameTime / timePerRun));

    PrintOutput("%-6d %8d %15.3f %15.3f %13d\n", order, frameSize, frameTime * 1e3f, timePerRun * 1e3f, numSources);

    iplAmbisonicsBinauralEffectRelease(&effect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);
}

BENCHMARK(ambisonicsbinaural)
{
    PrintOutput("Running benchmark: Ambisonics Binaural Rendering...\n");
    PrintOutput("%-6s %8s %18s %18s %13s\n", "Order", "Frames", "Frame Time (ms)", "Effect Time (ms)", "Max Sources");

    BenchmarkAmbisonicsBinauralForOrder(0, 512);
    BenchmarkAmbisonicsBinauralForOrder(1, 512);
    BenchmarkAmbisonicsBinauralForOrder(2, 512);
    BenchmarkAmbisonicsBinauralForOrder(3, 512);

    BenchmarkAmbisonicsBinauralForOrder(0, 1024);
    BenchmarkAmbisonicsBinauralForOrder(1, 1024);
    BenchmarkAmbisonicsBinauralForOrder(2, 1024);
    BenchmarkAmbisonicsBinauralForOrder(3, 1024);

    BenchmarkAmbisonicsBinauralForOrder(0, 2048);
    BenchmarkAmbisonicsBinauralForOrder(1, 2048);
    BenchmarkAmbisonicsBinauralForOrder(2, 2048);
    BenchmarkAmbisonicsBinauralForOrder(3, 2048);
}
