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

void BenchmarkBinauralWithInterpolation(int numChannels,
                                        IPLHRTFInterpolation interpolation,
                                        IPLbool enableSpatialBlend,
                                        float spatialBlend,
                                        IPLHRTFSettings hrtfParams,
                                        int frameSize)
{
    const int kNumRuns = 100000;
    const int kSamplingRate = 48000;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings dspParams = { kSamplingRate, frameSize };

    IPLHRTF hrtf = nullptr;
    iplHRTFCreate(context, &dspParams, &hrtfParams, &hrtf);
    if (hrtf == nullptr)
    {
        PrintOutput("Could not load HRTF: %s\n", hrtfParams.type == IPL_HRTFTYPE_SOFA ? hrtfParams.sofaFileName : "default");
        iplContextRelease(&context);
        return;
    }

    float* inData[2];
    float* outData[2];

    inData[0] = new float[frameSize];
    inData[1] = new float[frameSize];
    memset(inData[0], 0, sizeof(int) * frameSize);
    memset(inData[1], 0, sizeof(int) * frameSize);

    outData[0] = new float[frameSize];
    outData[1] = new float[frameSize];
    memset(outData[0], 0, sizeof(int) * frameSize);
    memset(outData[1], 0, sizeof(int) * frameSize);

    FillRandomData(inData[0], frameSize);
    FillRandomData(inData[1], frameSize);

    IPLBinauralEffect effect = nullptr;
    IPLBinauralEffectSettings effectSettings{ hrtf };
    iplBinauralEffectCreate(context, &dspParams, &effectSettings, &effect);

    IPLAudioBuffer inBuffer{ numChannels, frameSize, inData };
    IPLAudioBuffer outBuffer{ 2, frameSize, outData };

    IPLVector3 direction = { 1.0f, 0.0f, 0.0f };

    double timePerRun;

    Timer timer;
    timer.start();

    for (auto i = 0; i < kNumRuns; ++i)
    {
        IPLBinauralEffectParams params{ direction, interpolation, spatialBlend, hrtf };
        iplBinauralEffectApply(effect, &params, &inBuffer, &outBuffer);
    }

    timePerRun = timer.elapsedSeconds() / kNumRuns;

    iplBinauralEffectRelease(&effect);
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    auto frameTime = static_cast<double>(frameSize) / static_cast<double>(kSamplingRate);
    auto cpuUsage = (timePerRun / frameTime) * 100.0;
    auto numSources = static_cast<int>(floor(frameTime / timePerRun));

#if defined(IPL_OS_ANDROID)
    gLog().message(MessageSeverity::Info, "%-20s %-20s %9.1f %8.1f%% %9s %9d %13d\n",
        (numChannels == 1) ? "Mono" : "Stereo",
        (interpolation == IPL_HRTFINTERPOLATION_BILINEAR) ? "Bilinear" : "Nearest",
        spatialBlend,
        cpuUsage,
        (hrtfParams.type == IPL_HRTFTYPE_DEFAULT) ? "Default" : "SOFA",
        frameSize,
        numSources);
#else
    PrintOutput("%-20s %-20s %9.1f %8.1f%% %9s %9d %13d\n",
        (numChannels == 1) ? "Mono" : "Stereo",
        (interpolation == IPL_HRTFINTERPOLATION_BILINEAR) ? "Bilinear" : "Nearest",
        spatialBlend,
        cpuUsage,
        (hrtfParams.type == IPL_HRTFTYPE_DEFAULT) ? "Default" : "SOFA",
        frameSize,
        numSources);
#endif

    delete[] inData[0];
    delete[] outData[0];
    delete[] outData[1];
}

BENCHMARK(binaural)
{
    PrintOutput("Running benchmark: Object-Based Binaural Rendering...\n");
    PrintOutput("%-20s %-20s %9s %9s %9s %9s %13s\n", "Input Format", "Interpolation", "Blend", "CPU", "Mode", "Frames", "Max Sources");

    IPLbool enableSpatialBlend = IPL_TRUE;

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 1.0f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 1.0f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 1.0f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 1.0f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 0.5f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 0.5f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 0.5f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 0.5f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 1.0f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 1.0f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 1.0f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 1.0f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 0.5f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 0.5f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 0.5f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(2, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 0.5f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 1.0f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_NEAREST, enableSpatialBlend, 1.0f, hrtfParams, 1024);
    }

    {
        IPLHRTFSettings hrtfParams{ IPL_HRTFTYPE_SOFA, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
        hrtfParams.sofaFileName = "../../data/hrtf/sadie_d1.sofa";
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 1.0f, hrtfParams, 512);
        BenchmarkBinauralWithInterpolation(1, IPL_HRTFINTERPOLATION_BILINEAR, enableSpatialBlend, 1.0f, hrtfParams, 1024);
    }

    PrintOutput("\n");
}
