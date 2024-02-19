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

void BenchmarkDirectSoundEffectWithOptions(bool applyTransmission, IPLTransmissionType transmissionType, int numChannels)
{
    if (numChannels > 2)
    {
        printf("Number of channels must be either 1 or 2.\n");
        return;
    }

    const int kNumRuns = 100000;
    const int kSamplingRate = 48000;
    const int kFrameSize = 1024;

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

    IPLAudioSettings renderSettings = { kSamplingRate, kFrameSize };

    IPLDirectEffect directSoundEffect = nullptr;
    IPLDirectEffectSettings effectSettings{ numChannels };
    iplDirectEffectCreate(context, &renderSettings, &effectSettings, &directSoundEffect);

    float inData0[kFrameSize] = { 0 };
    float inData1[kFrameSize] = { 0 };
    float* inDatad[] = { inData0, inData1 };
    float outData0[kFrameSize] = { 0 };
    float outData1[kFrameSize] = { 0 };
    float* outDatad[] = { outData0, outData1 };
    FillRandomData(inData0, kFrameSize);
    FillRandomData(inData1, kFrameSize);

    IPLAudioBuffer inBuffer{ numChannels, kFrameSize, inDatad };
    IPLAudioBuffer outBuffer{ numChannels, kFrameSize, outDatad };

    IPLDirectEffectParams directParams;
    directParams.flags = (IPLDirectEffectFlags) (IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION | IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION);
    directParams.distanceAttenuation = 1.0f;
    directParams.airAbsorption[0] = 0.1f;
    directParams.airAbsorption[1] = 0.2f;
    directParams.airAbsorption[2] = 0.3f;
    directParams.occlusion = 0.5f;
    directParams.transmission[0] = 0.1f;
    directParams.transmission[1] = 0.2f;
    directParams.transmission[2] = 0.3f;
    directParams.transmissionType = transmissionType;

    if (applyTransmission)
        directParams.flags = (IPLDirectEffectFlags) (directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);

    double timePerRun;

    Timer timer;
    timer.start();

    for (auto i = 0; i < kNumRuns; ++i)
    {
        // Changing transmission factor each run to get the worst case performance. Avoid caching
        // perf benefits. Comment the line below to get best case perf gains.
        directParams.transmission[0] = (i + .1f) / kNumRuns;

        iplDirectEffectApply(directSoundEffect, &directParams, &inBuffer, &outBuffer);
    }

    timePerRun = timer.elapsedSeconds() / kNumRuns;

    iplDirectEffectRelease(&directSoundEffect);
    iplContextRelease(&context);

    auto frameTime = static_cast<double>(kFrameSize) / static_cast<double>(kSamplingRate);
    auto cpuUsage = (timePerRun / frameTime) * 100.0;
    auto maxChannels = static_cast<int>(floor(frameTime / timePerRun));

    auto outstring = "Off";
    if (applyTransmission && transmissionType == IPL_TRANSMISSIONTYPE_FREQINDEPENDENT)
        outstring = "Volume Scaling";
    else if (applyTransmission && transmissionType == IPL_TRANSMISSIONTYPE_FREQDEPENDENT)
        outstring = "Frequency Scaling";

    PrintOutput("%-20s %3.4f %18d %18d\n",
        outstring, cpuUsage, numChannels, maxChannels);
}

BENCHMARK(directsoundeffect)
{
    PrintOutput("Running benchmark: Direct Sound Effect...\n");
    PrintOutput("%-2s %12s %18s %18s\n", "Transmission Mode", "CPU Usage", "In Channels", "Max Channels");

    {
        int numChannels = 1;
        BenchmarkDirectSoundEffectWithOptions(false, IPL_TRANSMISSIONTYPE_FREQINDEPENDENT, numChannels);
        BenchmarkDirectSoundEffectWithOptions(true, IPL_TRANSMISSIONTYPE_FREQINDEPENDENT, numChannels);
        BenchmarkDirectSoundEffectWithOptions(true, IPL_TRANSMISSIONTYPE_FREQDEPENDENT, numChannels);
    }

    {
        int numChannels = 2;
        BenchmarkDirectSoundEffectWithOptions(false, IPL_TRANSMISSIONTYPE_FREQINDEPENDENT, numChannels);
        BenchmarkDirectSoundEffectWithOptions(true, IPL_TRANSMISSIONTYPE_FREQINDEPENDENT, numChannels);
        BenchmarkDirectSoundEffectWithOptions(true, IPL_TRANSMISSIONTYPE_FREQDEPENDENT, numChannels);
    }

    PrintOutput("\n");
}
