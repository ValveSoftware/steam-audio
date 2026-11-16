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

#include <vector>

#include <phonon.h>

namespace {

// Simulates the reflections in an empty box, with the source and the listener not moving, as many times as there are
// reverb scales in a row, and returns the reverb times (in the first band) that each simulation gives.
std::vector<float> simulate(float reverbScale,
                            int numRuns,
                            IPLReflectionEffectType effectType)
{
    const auto samplingRate = 48000;
    const auto frameSize = 1024;
    const auto duration = 4.0f;

    IPLContextSettings contextSettings{};
    contextSettings.version = STEAMAUDIO_VERSION;
    IPLContext context = nullptr;
    iplContextCreate(&contextSettings, &context);

    IPLSceneSettings sceneSettings{};
    sceneSettings.type = IPL_SCENETYPE_DEFAULT;
    IPLScene scene = nullptr;
    iplSceneCreate(context, &sceneSettings, &scene);

    // A box of 20 x 10 x 6 m, with walls that absorb 15% of the energy.
    IPLVector3 vertices[8];
    for (auto i = 0; i < 8; ++i)
    {
        vertices[i] = { (i & 1) ? 20.0f : 0.0f, (i & 2) ? 10.0f : 0.0f, (i & 4) ? 6.0f : 0.0f };
    }

    const int quads[6][4] = { {0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3} };
    std::vector<IPLTriangle> triangles;
    std::vector<IPLint32> materialIndices;
    for (const auto& q : quads)
    {
        triangles.push_back({ { q[0], q[1], q[2] } });
        triangles.push_back({ { q[0], q[2], q[3] } });
        materialIndices.push_back(0);
        materialIndices.push_back(0);
    }

    IPLMaterial material{};
    for (auto band = 0; band < IPL_NUM_BANDS; ++band)
    {
        material.absorption[band] = 0.15f;
    }
    material.scattering = 0.5f;

    IPLStaticMeshSettings meshSettings{};
    meshSettings.numVertices = 8;
    meshSettings.numTriangles = 12;
    meshSettings.numMaterials = 1;
    meshSettings.vertices = vertices;
    meshSettings.triangles = triangles.data();
    meshSettings.materialIndices = materialIndices.data();
    meshSettings.materials = &material;
    IPLStaticMesh mesh = nullptr;
    iplStaticMeshCreate(scene, &meshSettings, &mesh);
    iplStaticMeshAdd(mesh, scene);
    iplSceneCommit(scene);

    IPLSimulationSettings simulationSettings{};
    simulationSettings.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
    simulationSettings.sceneType = IPL_SCENETYPE_DEFAULT;
    simulationSettings.reflectionType = effectType;
    simulationSettings.maxNumRays = 4096;
    simulationSettings.numDiffuseSamples = 32;
    simulationSettings.maxDuration = duration;
    simulationSettings.maxOrder = 1;
    simulationSettings.maxNumSources = 1;
    simulationSettings.numThreads = 2;
    simulationSettings.rayBatchSize = 64;
    simulationSettings.samplingRate = samplingRate;
    simulationSettings.frameSize = frameSize;
    IPLSimulator simulator = nullptr;
    iplSimulatorCreate(context, &simulationSettings, &simulator);
    iplSimulatorSetScene(simulator, scene);
    iplSimulatorCommit(simulator);

    IPLSourceSettings sourceSettings{};
    sourceSettings.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
    IPLSource source = nullptr;
    iplSourceCreate(simulator, &sourceSettings, &source);
    iplSourceAdd(source, simulator);
    iplSimulatorCommit(simulator);

    IPLSimulationSharedInputs sharedInputs{};
    sharedInputs.listener = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 17, 5, 3 } };
    sharedInputs.numRays = 4096;
    sharedInputs.numBounces = 128;
    sharedInputs.duration = duration;
    sharedInputs.order = 1;
    sharedInputs.irradianceMinDistance = 1.0f;
    iplSimulatorSetSharedInputs(simulator, IPL_SIMULATIONFLAGS_REFLECTIONS, &sharedInputs);

    IPLSimulationInputs inputs{};
    inputs.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
    inputs.source = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 3, 5, 3 } };
    inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
    inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
    inputs.directivity = { 0.0f, 1.0f, nullptr, nullptr };
    for (auto band = 0; band < IPL_NUM_BANDS; ++band)
    {
        inputs.reverbScale[band] = reverbScale;
    }
    inputs.hybridReverbTransitionTime = 1.0f;
    inputs.hybridReverbOverlapPercent = 0.25f;
    iplSourceSetInputs(source, IPL_SIMULATIONFLAGS_REFLECTIONS, &inputs);

    std::vector<float> reverbTimes;
    for (auto i = 0; i < numRuns; ++i)
    {
        iplSimulatorRunReflections(simulator);

        IPLSimulationOutputs outputs{};
        iplSourceGetOutputs(source, IPL_SIMULATIONFLAGS_REFLECTIONS, &outputs);
        reverbTimes.push_back(outputs.reflections.reverbTimes[0]);
    }

    iplSourceRemove(source, simulator);
    iplSourceRelease(&source);
    iplSimulatorRelease(&simulator);
    iplStaticMeshRemove(mesh, scene);
    iplStaticMeshRelease(&mesh);
    iplSceneRelease(&scene);
    iplContextRelease(&context);

    return reverbTimes;
}

}

// The reverb scale multiplies the reverb times that are estimated. The source and the listener do not move here, so
// the simulator keeps accumulating the energy field over the runs, and the scale has to apply to the estimate of every
// run, not to what the previous runs left in the field.
TEST_CASE("The reverb scale does not accumulate over the simulations", "[ReverbScale]")
{
    for (const auto effectType : { IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, IPL_REFLECTIONEFFECTTYPE_HYBRID })
    {
        const auto unscaled = simulate(1.0f, 1, effectType)[0];
        const auto scaled = simulate(2.0f, 6, effectType);

        for (size_t run = 0; run < scaled.size(); ++run)
        {
            INFO("effect type " << effectType << ", run " << run << ": " << scaled[run] << " s, unscaled " << unscaled << " s");
            CHECK(scaled[run] == Approx(2.0f * unscaled).epsilon(0.1));
        }
    }
}
