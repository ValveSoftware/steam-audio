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

#include <catch.hpp>

#include <cmath>
#include <vector>

#include <phonon.h>

namespace {

const auto kSamplingRate = 48000;
const auto kFrameSize = 512;
const auto kDuration = 0.5f;

// A closed box 3 m wide, with a source and a listener that do not move, simulated for reflections. The simulator
// averages the passes made since the listener, the source or the scene last changed.
class Room
{
public:
    Room()
    {
        IPLContextSettings contextSettings{};
        contextSettings.version = STEAMAUDIO_VERSION;
        iplContextCreate(&contextSettings, &mContext);

        IPLSceneSettings sceneSettings{};
        sceneSettings.type = IPL_SCENETYPE_DEFAULT;
        iplSceneCreate(mContext, &sceneSettings, &mScene);

        const auto h = 1.5f;
        IPLVector3 vertices[8] = { {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h} };
        IPLTriangle triangles[12] = { {{0, 1, 2}}, {{0, 2, 3}}, {{4, 6, 5}}, {{4, 7, 6}}, {{0, 4, 5}}, {{0, 5, 1}},
                                      {{3, 2, 6}}, {{3, 6, 7}}, {{0, 3, 7}}, {{0, 7, 4}}, {{1, 5, 6}}, {{1, 6, 2}} };
        IPLint32 materialIndices[12] = {};
        IPLMaterial material{ {0.1f, 0.1f, 0.1f}, 0.5f, {0.0f, 0.0f, 0.0f} };

        IPLStaticMeshSettings meshSettings{};
        meshSettings.numVertices = 8;
        meshSettings.numTriangles = 12;
        meshSettings.numMaterials = 1;
        meshSettings.vertices = vertices;
        meshSettings.triangles = triangles;
        meshSettings.materialIndices = materialIndices;
        meshSettings.materials = &material;
        iplStaticMeshCreate(mScene, &meshSettings, &mMesh);
        iplStaticMeshAdd(mMesh, mScene);
        iplSceneCommit(mScene);

        IPLSimulationSettings simulationSettings{};
        simulationSettings.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
        simulationSettings.sceneType = IPL_SCENETYPE_DEFAULT;
        simulationSettings.reflectionType = IPL_REFLECTIONEFFECTTYPE_HYBRID;
        simulationSettings.maxNumRays = 2048;
        simulationSettings.numDiffuseSamples = 1024;
        simulationSettings.maxDuration = kDuration;
        simulationSettings.maxOrder = 0;
        simulationSettings.maxNumSources = 4;
        simulationSettings.numThreads = 4;
        simulationSettings.samplingRate = kSamplingRate;
        simulationSettings.frameSize = kFrameSize;
        iplSimulatorCreate(mContext, &simulationSettings, &mSimulator);
        iplSimulatorSetScene(mSimulator, mScene);

        IPLSourceSettings sourceSettings{};
        sourceSettings.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
        iplSourceCreate(mSimulator, &sourceSettings, &mSource);
        iplSourceAdd(mSource, mSimulator);
        iplSimulatorCommit(mSimulator);
    }

    ~Room()
    {
        iplSourceRelease(&mSource);
        iplSimulatorRelease(&mSimulator);
        iplStaticMeshRelease(&mMesh);
        iplSceneRelease(&mScene);
        iplContextRelease(&mContext);
    }

    void simulate(int numBounces)
    {
        const IPLCoordinateSpace3 space{ {1, 0, 0}, {0, 1, 0}, {0, 0, -1}, {0.2f, 0.1f, 0.1f} };

        IPLSimulationInputs inputs{};
        inputs.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
        inputs.source = space;
        inputs.directivity.dipolePower = 1.0f;
        inputs.reverbScale[0] = inputs.reverbScale[1] = inputs.reverbScale[2] = 1.0f;
        inputs.hybridReverbTransitionTime = kDuration;
        inputs.hybridReverbOverlapPercent = 0.25f;
        iplSourceSetInputs(mSource, IPL_SIMULATIONFLAGS_REFLECTIONS, &inputs);

        IPLSimulationSharedInputs shared{};
        shared.listener = space;
        shared.numRays = 2048;
        shared.numBounces = numBounces;
        shared.duration = kDuration;
        shared.order = 0;
        shared.irradianceMinDistance = 0.25f;
        iplSimulatorSetSharedInputs(mSimulator, IPL_SIMULATIONFLAGS_REFLECTIONS, &shared);

        iplSimulatorRunReflections(mSimulator);
    }

    // Energy of the impulse response between 0.15 s and 0.4 s, in dB.
    double tailLevel()
    {
        IPLSimulationOutputs outputs{};
        iplSourceGetOutputs(mSource, IPL_SIMULATIONFLAGS_REFLECTIONS, &outputs);

        IPLAudioSettings audioSettings{ kSamplingRate, kFrameSize };
        IPLReflectionEffectSettings effectSettings{};
        effectSettings.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        effectSettings.irSize = static_cast<int>(kDuration * kSamplingRate);
        effectSettings.numChannels = 1;
        IPLReflectionEffect effect = nullptr;
        iplReflectionEffectCreate(mContext, &audioSettings, &effectSettings, &effect);

        IPLAudioBuffer in{}, out{};
        iplAudioBufferAllocate(mContext, 1, kFrameSize, &in);
        iplAudioBufferAllocate(mContext, 1, kFrameSize, &out);

        IPLReflectionEffectParams params{};
        params.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        params.ir = outputs.reflections.ir;
        params.numChannels = 1;
        params.irSize = effectSettings.irSize;

        auto sum = 0.0;
        for (auto position = 0; position < effectSettings.irSize; position += kFrameSize)
        {
            for (auto i = 0; i < kFrameSize; ++i)
            {
                in.data[0][i] = (position == 0 && i == 0) ? 1.0f : 0.0f;
            }

            iplReflectionEffectApply(effect, &params, &in, &out, nullptr);

            for (auto i = 0; i < kFrameSize; ++i)
            {
                const auto time = static_cast<double>(position + i) / kSamplingRate;
                if (0.15 <= time && time < 0.4)
                {
                    sum += static_cast<double>(out.data[0][i]) * out.data[0][i];
                }
            }
        }

        iplAudioBufferFree(mContext, &in);
        iplAudioBufferFree(mContext, &out);
        iplReflectionEffectRelease(&effect);

        return 10.0 * std::log10(std::max(sum, 1e-30));
    }

private:
    IPLContext mContext = nullptr;
    IPLScene mScene = nullptr;
    IPLStaticMesh mMesh = nullptr;
    IPLSimulator mSimulator = nullptr;
    IPLSource mSource = nullptr;
};

}

// The simulator averages the energy fields of successive passes, and starts again when the listener, a source or the scene
// changes. The number of bounces, rays, the duration... are not one of these: after a change of any of them, the new
// passes were averaged with every pass made with the old settings since the last move, and weighed 1 / n. With 16 bounces,
// which leave nothing of the tail of a small room, then 128, the tail stayed under that of a simulator that had 128 from
// the start for as many passes as there were before the change (13 dB under after one pass, 9 dB after three, 3 dB after twenty,
// measured with twenty passes made with 16 bounces).
TEST_CASE("A change of the settings of the reflection simulation restarts the average of its passes", "[Simulation]")
{
    const auto numPassesAfter = 3;

    double fresh = 0.0;
    {
        Room room;
        for (auto i = 0; i < numPassesAfter; ++i)
        {
            room.simulate(128);
        }

        fresh = room.tailLevel();
    }

    Room room;
    for (auto i = 0; i < 20; ++i)
    {
        room.simulate(16);
    }

    room.tailLevel();

    for (auto i = 0; i < numPassesAfter; ++i)
    {
        room.simulate(128);
    }

    const auto changed = room.tailLevel();

    INFO("tail energy: " << changed << " dB after " << numPassesAfter << " passes with 128 bounces following 20 with 16; " << fresh << " dB with 128 from the start");
    CHECK(std::fabs(changed - fresh) < 3.0);
}
