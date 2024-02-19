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
#include <phonon.h>

#include <vector>

#include "phonon_perf.h"

void benchmarkPathEffectInitialization(int order, int frameSize)
{
    const int kNumRuns = 100;
    const int kSamplingRate = 48000;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, frameSize };

    IPLHRTFSettings hrtfSettings{};
    hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;
    hrtfSettings.volume = 1.0f;

    IPLHRTF hrtf = nullptr;
    iplHRTFCreate(context, &dspParams, &hrtfSettings, &hrtf);

    IPLPathEffect effect = nullptr;
    IPLPathEffectSettings settings{};
    settings.maxOrder = order;
    settings.spatialize = IPL_TRUE;
    settings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
    settings.hrtf = hrtf;

    double timePerRun = .0;
    ipl::Timer timer;

    for (auto i = 0; i < kNumRuns; ++i)
    {
        timer.start();
        iplPathEffectCreate(context, &dspParams, &settings, &effect);

        timePerRun += timer.elapsedMilliseconds();
        iplPathEffectRelease(&effect);
    }

    timePerRun /= kNumRuns;
    auto frameTime = static_cast<double>(frameSize) / static_cast<double>(kSamplingRate);

    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    PrintOutput("%-6d %8d %15.3f %15.3f\n", order, frameSize, frameTime * 1e3f, timePerRun);
}

void benchmarkPathEffectWithParams(int order, int frameSize)
{
    const int kNumRuns = 10000;
    const int kSamplingRate = 48000;
    const int numChannels = (order + 1) * (order + 1);
    const int kNumBands = 3;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, frameSize };

    IPLHRTFSettings hrtfSettings{};
    hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;
    hrtfSettings.volume = 1.0f;

    IPLHRTF hrtf = nullptr;
    iplHRTFCreate(context, &dspParams, &hrtfSettings, &hrtf);

    IPLVector3 listenerPosition = { 0.0f, 0.0f, 0.0f };
    IPLVector3 listenerAhead = { 0.0f, 0.0f, -1.0f };
    IPLVector3 listenerUp = { 0.0f, 1.0f, 0.0f };

    float eqGains[kNumBands] = {1.0f, 0.5f, 0.25f};
    std::vector<float> coeffs(numChannels);
    for (auto i = 0u; i < coeffs.size(); ++i)
    {
        coeffs[i] = rand() / (10.0f * RAND_MAX);
    }

    float* inData[1];
    float* outData[2];

    inData[0] = new float[frameSize];
    memset(inData[0], 0, sizeof(int) * frameSize);

    outData[0] = new float[frameSize];
    outData[1] = new float[frameSize];
    memset(outData[0], 0, sizeof(int) * frameSize);
    memset(outData[1], 0, sizeof(int) * frameSize);

    FillRandomData(inData[0], frameSize);

    IPLAudioBuffer inBuffer{ 1, frameSize, inData };
    IPLAudioBuffer outBuffer{ 2, frameSize, outData };

    IPLPathEffectSettings settings{};
    settings.maxOrder = order;
    settings.spatialize = IPL_TRUE;
    settings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
    settings.hrtf = hrtf;

    IPLPathEffect effect = nullptr;
    iplPathEffectCreate(context, &dspParams, &settings, &effect);

    IPLPathEffectParams params{};
    params.order = order;
    memcpy(params.eqCoeffs, eqGains, 3 * sizeof(float));
    params.shCoeffs = coeffs.data();
    params.binaural = IPL_TRUE;
    params.hrtf = hrtf;
    params.listener.origin = IPLVector3{0.0f, 0.0f, 0.0f};
    params.listener.right = IPLVector3{1.0f, 0.0f, 0.0f};
    params.listener.up = IPLVector3{0.0f, 1.0f, 0.0f};
    params.listener.ahead = IPLVector3{0.0f, 0.0f, -1.0f};

    ipl::Timer timer{};
    timer.start();

    for (auto i = 0; i < kNumRuns; ++i)
    {
        iplPathEffectApply(effect, &params, &inBuffer, &outBuffer);
    }

    iplPathEffectRelease(&effect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    auto timePerRun = timer.elapsedSeconds() / kNumRuns;
    auto frameTime = static_cast<double>(frameSize) / static_cast<double>(kSamplingRate);
    auto numSources = static_cast<int>(floor(frameTime / timePerRun));

    PrintOutput("%-6d %8d %15.3f %15.3f %13d\n", order, frameSize, frameTime * 1e3f, timePerRun * 1e3f, numSources);

    delete[] inData[0];
    delete[] outData[0];
    delete[] outData[1];
}

BENCHMARK(patheffect)
{
    PrintOutput("Running benchmark: Path Effect Initialization...\n");
    PrintOutput("%-6s %8s %18s %18s\n", "Order", "Frames", "Frame Time (ms)", "Init Time (ms)");

    benchmarkPathEffectInitialization(0, 512);
    benchmarkPathEffectInitialization(1, 512);
    benchmarkPathEffectInitialization(2, 512);
    benchmarkPathEffectInitialization(3, 512);

    benchmarkPathEffectInitialization(0, 1024);
    benchmarkPathEffectInitialization(1, 1024);
    benchmarkPathEffectInitialization(2, 1024);
    benchmarkPathEffectInitialization(3, 1024);

    benchmarkPathEffectInitialization(0, 2048);
    benchmarkPathEffectInitialization(1, 2048);
    benchmarkPathEffectInitialization(2, 2048);
    benchmarkPathEffectInitialization(3, 2048);

    PrintOutput("\nRunning benchmark: Path Effect Apply...\n");
    PrintOutput("%-6s %8s %18s %18s %13s\n", "Order", "Frames", "Frame Time (ms)", "Effect Time (ms)", "Max Sources");

    benchmarkPathEffectWithParams(0, 512);
    benchmarkPathEffectWithParams(1, 512);
    benchmarkPathEffectWithParams(2, 512);
    benchmarkPathEffectWithParams(3, 512);

    benchmarkPathEffectWithParams(0, 1024);
    benchmarkPathEffectWithParams(1, 1024);
    benchmarkPathEffectWithParams(2, 1024);
    benchmarkPathEffectWithParams(3, 1024);

    benchmarkPathEffectWithParams(0, 2048);
    benchmarkPathEffectWithParams(1, 2048);
    benchmarkPathEffectWithParams(2, 2048);
    benchmarkPathEffectWithParams(3, 2048);
}
