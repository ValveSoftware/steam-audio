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
//#define WAIT_FOR_BENCHMARK

#define SMPL_FRAME_TIME 21.33f
//#define SMPL_FRAME_TIME 2.667f
//#define SMPL_FRAME_TIME 5.333f
//#define SMPL_FRAME_TIME 10.66f
//#define SMPL_FRAME_TIME 0.6667f
#include <profiler.h>
#include <containers.h>
using namespace ipl;

#include <phonon.h>

#include <chrono>
#include <thread>

#include "phonon_perf.h"

const int gSamplingRate = 48000;
const int gFrameSize = 1024;
//const int gFrameSize = 256;
//const int gFrameSize = 64;
//const int gFrameSize = 32;
void AudioThread(const int sources, IPLContext context, IPLSimulator simulator, IPLSimulationSettings settings)
{
    IPLSourceSettings sourceSettings;
    sourceSettings.flags = settings.flags;

    IPLSource source = nullptr;
    iplSourceCreate(simulator, &sourceSettings, &source);
    iplSourceAdd(source, simulator);
    iplSimulatorCommit(simulator);

    IPLSimulationSharedInputs sharedInputs{};
    sharedInputs.listener.origin = IPLVector3{ 0, 1, 0 };
    sharedInputs.numRays = settings.maxNumRays;
    sharedInputs.numBounces = 1;
    sharedInputs.duration = settings.maxDuration;
    sharedInputs.order = settings.maxOrder;
    sharedInputs.irradianceMinDistance = 1.0f;
    iplSimulatorSetSharedInputs(simulator, IPL_SIMULATIONFLAGS_REFLECTIONS, &sharedInputs);

    IPLSimulationInputs inputs{};
    inputs.flags = (IPLSimulationFlags) (IPL_SIMULATIONFLAGS_REFLECTIONS);
    inputs.directFlags = (IPLDirectSimulationFlags) 0;
    inputs.source.origin = IPLVector3{ 0, 1, 0 };
    inputs.source.ahead = IPLVector3{ 0, 0, -1 };
    inputs.source.up = IPLVector3{ 0, 1, 0 };
    inputs.source.right = IPLVector3{ 1, 0, 0 };
    inputs.distanceAttenuationModel = IPLDistanceAttenuationModel{};
    inputs.airAbsorptionModel = IPLAirAbsorptionModel{};
    inputs.directivity = IPLDirectivity{};
    inputs.occlusionType = IPL_OCCLUSIONTYPE_RAYCAST;
    inputs.occlusionRadius = 0.0f;
    inputs.numOcclusionSamples = 0;
    inputs.reverbScale[0] = 1.0f;
    inputs.reverbScale[1] = 1.0f;
    inputs.reverbScale[2] = 1.0f;
    inputs.hybridReverbTransitionTime = 1.0f;
    inputs.hybridReverbOverlapPercent = 0.25f;
    iplSourceSetInputs(source, IPL_SIMULATIONFLAGS_REFLECTIONS, &inputs);

    iplSimulatorRunReflections(simulator);

    IPLSimulationOutputs outputs{};
    outputs.reflections.type = settings.reflectionType;
    outputs.reflections.tanDevice = settings.tanDevice;
    iplSourceGetOutputs(source, IPL_SIMULATIONFLAGS_REFLECTIONS, &outputs);

    auto numChannels = (settings.maxOrder + 1) * (settings.maxOrder + 1);

    IPLAudioBuffer dryAudio;
    IPLAudioBuffer mixedWetAudio;
    iplAudioBufferAllocate(context, 1, gFrameSize, &dryAudio);
    iplAudioBufferAllocate(context, numChannels, gFrameSize, &mixedWetAudio);
    FillRandomData(dryAudio.data[0], gFrameSize);

    IPLAudioSettings audioSettings{};
    audioSettings.samplingRate = settings.samplingRate;
    audioSettings.frameSize = settings.frameSize;

    IPLReflectionEffectSettings effectSettings{};
    effectSettings.type = settings.reflectionType;
    if (settings.reflectionType == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION)
    {
        effectSettings.numChannels = numChannels;
        effectSettings.irSize = (int) ceilf(settings.maxDuration * settings.samplingRate);
    }

    IPLReflectionMixer mixer = nullptr;
    iplReflectionMixerCreate(context, &audioSettings, &effectSettings, &mixer);

    vector<IPLReflectionEffect> effects(sources);
    for (auto source = 0; source < sources; ++source)
        iplReflectionEffectCreate(context, &audioSettings, &effectSettings, &effects[source]);

    // Warmup Run
    {
        for (auto source = 0; source < sources; ++source)
            iplReflectionEffectApply(effects[source], &outputs.reflections, &dryAudio, &mixedWetAudio, mixer);

        iplReflectionMixerApply(mixer, &outputs.reflections, &mixedWetAudio);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        for (auto i = 0; i < 20; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            for (auto source = 0; source < sources; ++source)
                iplReflectionEffectApply(effects[source], &outputs.reflections, &dryAudio, &mixedWetAudio, mixer);

            iplReflectionMixerApply(mixer, &outputs.reflections, &mixedWetAudio);
        }
    }

    // When running together with another competing workload or benchmark, increase kNumRuns such that kNumRuns * 20 ms
    // exceeds the time the benchmark requires to run to completion, for a single representative workload,
    // e.g., 32 sources of 2nd order Ambisonics

    int underflowCount = 0;
    int kNumRuns = 100;
    double totalTime = .0;
    double elapsedTime = 0.0;
    double frameTime = 0.0;
    double sleepTime = 0.0;
    double frameTimeMicroS = SMPL_FRAME_TIME * 1000.0;
    Timer timer;
    int channels = sources * numChannels;


    for (int i = 0; i < kNumRuns; ++i)
    {
        if (frameTime > frameTimeMicroS) frameTime = frameTimeMicroS;
        sleepTime = floor(frameTimeMicroS - frameTime);
        std::this_thread::sleep_for(std::chrono::microseconds((int)(sleepTime)));


        timer.start();

        for (auto source = 0; source < sources; ++source)
            iplReflectionEffectApply(effects[source], &outputs.reflections, &dryAudio, &mixedWetAudio, mixer);

        iplReflectionMixerApply(mixer, &outputs.reflections, &mixedWetAudio);

        elapsedTime = (timer.elapsedMicroseconds());

        totalTime += elapsedTime;
        if (sleepTime <= 0.0f) {
            underflowCount += 1;
            //PrintOutput("Underflow error %d\r", underflowCount);
        }

        frameTime = elapsedTime;

    }
    auto perFrameTime = (totalTime / kNumRuns) / 1000.0;
    //if (underflowCount > 0)
    //    PrintOutput("\n");

    PrintOutput("%-10d %-10d %8.1f s %10d %8.1f ms\n", sources, channels, settings.maxDuration, settings.maxOrder, perFrameTime);

    iplAudioBufferFree(context, &dryAudio);
    iplAudioBufferFree(context, &mixedWetAudio);

    for (auto source = 0; source < sources; ++source)
        iplReflectionEffectRelease(&effects[source]);

    iplReflectionMixerRelease(&mixer);

    iplSourceRemove(source, simulator);
    iplSourceRelease(&source);
}

void BenchmarkConvolutionForSettings(const std::string& fileName, IPLSceneType sceneType, IPLReflectionEffectType indirectType,
    IPLContext context, const int sources, const float duration, const int order)
{
    IPLOpenCLDevice clDevice = nullptr;
    IPLRadeonRaysDevice rrDevice = nullptr;
    IPLTrueAudioNextDevice tanDevice = nullptr;

#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS) && defined(IPL_USES_TRUEAUDIONEXT)
    if (sceneType == IPL_SCENETYPE_RADEONRAYS || indirectType == IPL_REFLECTIONEFFECTTYPE_TAN)
    {
        IPLOpenCLDeviceList deviceList = nullptr;
        IPLOpenCLDeviceSettings openCLSettings{IPL_OPENCLDEVICETYPE_ANY, 8, 0.0f};
        iplOpenCLDeviceListCreate(context, &openCLSettings, &deviceList);

        iplOpenCLDeviceCreate(context, deviceList, 0, &clDevice);

        iplOpenCLDeviceListRelease(&deviceList);
    }

    if (sceneType == IPL_SCENETYPE_RADEONRAYS)
    {
        IPLRadeonRaysDeviceSettings rrSettings{};
        iplRadeonRaysDeviceCreate(clDevice, &rrSettings, &rrDevice);
    }

    if (indirectType == IPL_REFLECTIONEFFECTTYPE_TAN)
    {
        IPLTrueAudioNextDeviceSettings tanSettings{};
        tanSettings.frameSize = gFrameSize;
        tanSettings.irSize = (int) ceilf(duration * gSamplingRate);
        tanSettings.order = order;
        tanSettings.maxSources = sources;

        iplTrueAudioNextDeviceCreate(clDevice, &tanSettings, &tanDevice);
    }
#endif

    std::vector<float>   vertices;
    std::vector<int32_t> triangleIndices;
    std::vector<int>     materialIndices;

    LoadObj(fileName, vertices, triangleIndices, materialIndices);
    IPLMaterial material = { 0.1f, 0.1f, 0.1f, 0.5f, 1.0f, 1.0f, 1.0f };

    IPLAudioSettings renderSettings{};
    renderSettings.samplingRate = gSamplingRate;
    renderSettings.frameSize = gFrameSize;

    IPLSceneSettings sceneSettings{};
    sceneSettings.type = sceneType;
    sceneSettings.embreeDevice = nullptr;
    sceneSettings.radeonRaysDevice = rrDevice;

    IPLScene scene = nullptr;
    iplSceneCreate(context, &sceneSettings, &scene);

    IPLStaticMeshSettings staticMeshSettings{};
    staticMeshSettings.numVertices = static_cast<int>(vertices.size() / 3);
    staticMeshSettings.numTriangles = static_cast<int>(triangleIndices.size() / 3);
    staticMeshSettings.numMaterials = 1;
    staticMeshSettings.vertices = (IPLVector3*) vertices.data();
    staticMeshSettings.triangles = (IPLTriangle*) triangleIndices.data();
    staticMeshSettings.materialIndices = materialIndices.data();
    staticMeshSettings.materials = &material;

    IPLStaticMesh staticMesh = nullptr;
    iplStaticMeshCreate(scene, &staticMeshSettings, &staticMesh);
    iplStaticMeshAdd(staticMesh, scene);
    iplSceneCommit(scene);

    IPLSimulationSettings settings{};
    settings.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
    settings.sceneType = sceneType;
    settings.reflectionType = indirectType;
    settings.maxNumRays = 1024 * 8;
    settings.numDiffuseSamples = 1024;
    settings.maxDuration = duration;
    settings.maxOrder = order;
    settings.maxNumSources = sources;
    settings.numThreads = 1;
    settings.rayBatchSize = 1;
    settings.samplingRate = gSamplingRate;
    settings.frameSize = gFrameSize;
    settings.openCLDevice = clDevice;
    settings.radeonRaysDevice = rrDevice;
    settings.tanDevice = tanDevice;

    IPLSimulator simulator = nullptr;
    iplSimulatorCreate(context, &settings, &simulator);
    iplSimulatorSetScene(simulator, scene);

    AudioThread(sources, context, simulator, settings);

    iplSimulatorRelease(&simulator);
    iplStaticMeshRelease(&staticMesh);
    iplSceneRelease(&scene);

#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS) && defined(IPL_USES_TRUEAUDIONEXT)
    iplTrueAudioNextDeviceRelease(&tanDevice);
    iplRadeonRaysDeviceRelease(&rrDevice);
    iplOpenCLDeviceRelease(&clDevice);
#endif
}

void BenchmarkConvolutionForScene(const std::string& fileName, IPLSceneType sceneType, IPLReflectionEffectType indirectType)
{
    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

#if defined(WAIT_FOR_BENCHMARK)
    PrintOutput("Waiting...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(45000));
    PrintOutput("...starting.\n");
#endif

    for (auto duration = 1.0f; duration <= 2.0f; duration *= 2.0f)
        for (auto order = 0; order <= 2; ++order)
            for (auto sources = 2; sources <= 32; sources *= 2)
                BenchmarkConvolutionForSettings(fileName, sceneType, indirectType, context, sources, duration, order);

    iplContextRelease(&context);
}

BENCHMARK(convolution)
{
    PrintOutput("Buffer Size = %d Frame Time = %f ms\n", gFrameSize, SMPL_FRAME_TIME);

    PrintOutput("Running benchmark: Convolution (CPU)...\n");
    PrintOutput("%-10s %10s %10s %10s %10s\n", "#Sources", "#Channels", "Duration", "Order", "Time");
    BenchmarkConvolutionForScene("../../data/meshes/sponza.obj", IPL_SCENETYPE_DEFAULT, IPL_REFLECTIONEFFECTTYPE_CONVOLUTION);
    PrintOutput("\n");

#if defined(IPL_USES_TRUEAUDIONEXT)
    PrintOutput("Running benchmark: Convolution (Phonon + TAN)...\n");
    PrintOutput("%-10s %10s %10s %10s %10s\n", "#Sources", "#Channels", "Duration", "Order", "Time");
    BenchmarkConvolutionForScene("../../data/meshes/sponza.obj", IPL_SCENETYPE_DEFAULT, IPL_REFLECTIONEFFECTTYPE_TAN);
    PrintOutput("\n");
#endif

#if (defined(IPL_USES_TRUEAUDIONEXT) && defined(IPL_USES_RADEONRAYS))
    PrintOutput("Running benchmark: Convolution (Radeon Rays + TAN)...\n");
    PrintOutput("%-10s %10s %10s %10s %10s\n", "#Sources", "#Channels", "Duration", "Order", "Time");
    BenchmarkConvolutionForScene("../../data/meshes/sponza.obj", IPL_SCENETYPE_RADEONRAYS, IPL_REFLECTIONEFFECTTYPE_TAN);
    PrintOutput("\n");
#endif

}
