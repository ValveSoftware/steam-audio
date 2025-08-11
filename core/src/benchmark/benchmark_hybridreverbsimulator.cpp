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
#include <probe_generator.h>
#include <reflection_baker.h>
#include <reflection_simulator_factory.h>
#include <energy_field_factory.h>
#include <reconstructor_factory.h>
#include <impulse_response_factory.h>
#include <hybrid_reverb_estimator.h>
#include <baked_reflection_data.h>
#include <baked_reflection_simulator.h>
#include <hrtf_map.h>
#include <hrtf_database.h>
#include <ambisonics_binaural_effect.h>
#include <scene_factory.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkHybridReverbSimulatorForSettings(const std::string& fileName, const SceneType type, float spacing, float duration, int order, int frameSize)
{
    if (type == ipl::SceneType::RadeonRays)
    {
        PrintOutput("Not Supported: Radeon Rays\n");
        return;
    }

    // Create scene.
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto embree = (type == SceneType::Embree) ? make_shared<EmbreeDevice>() : nullptr;
    auto radeonRays = nullptr;
    auto scene = shared_ptr<IScene>(SceneFactory::create(type, nullptr, nullptr, nullptr, nullptr, nullptr, embree, radeonRays));
    Matrix4x4f localToWorldTransform;
    auto numTriangles = 0;

    {
        std::vector<float>   vertices;
        std::vector<int32_t> triangleIndices;
        std::vector<int>     materialIndices;

        LoadObj(fileName, vertices, triangleIndices, materialIndices);
        numTriangles = static_cast<int>(triangleIndices.size()) / 3;

        float xmin = std::numeric_limits<float>().max();
        float ymin = std::numeric_limits<float>().max();
        float zmin = std::numeric_limits<float>().max();
        float xmax = std::numeric_limits<float>().lowest();
        float ymax = std::numeric_limits<float>().lowest();
        float zmax = std::numeric_limits<float>().lowest();

        for (auto i = 0u; i < vertices.size(); i += 3)
        {
            xmin = std::min(xmin, vertices[i]);
            ymin = std::min(ymin, vertices[i + 1]);
            zmin = std::min(zmin, vertices[i + 2]);

            xmax = std::max(xmax, vertices[i]);
            ymax = std::max(ymax, vertices[i + 1]);
            zmax = std::max(zmax, vertices[i + 2]);
        }

        localToWorldTransform.identity();
        localToWorldTransform(0, 3) = (xmin + xmax) / 2;
        localToWorldTransform(1, 3) = (ymin + ymax) / 2;
        localToWorldTransform(2, 3) = (zmin + zmax) / 2;
        localToWorldTransform(0, 0) = (xmax - xmin);
        localToWorldTransform(1, 1) = (ymax - ymin);
        localToWorldTransform(2, 2) = (zmax - zmin);

        Material material;
        for (auto iBand = 0; iBand < Bands::kNumBands; ++iBand)
            material.absorption[iBand] = 0.1f;
        material.scattering = 0.5f;
        for (auto iBand = 0; iBand < Bands::kNumBands; ++iBand)
            material.transmission[iBand] = 1.0f;

        auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size() / 3), static_cast<int>(triangleIndices.size() / 3), 1,
            reinterpret_cast<Vector3f*>(vertices.data()), (Triangle*)triangleIndices.data(),
            materialIndices.data(), &material);

        scene->addStaticMesh(staticMesh);
        scene->commit();
    }

    // Generate probes.
    ProbeBatch probeBatch;
    ipl::Vector3f listener;
    {
        Timer timer;
        timer.start();

        ProbeArray probeArray;
        ProbeGenerator::generateProbes(*scene, localToWorldTransform, ProbeGenerationType::UniformFloor, spacing, 1.5f, probeArray);
        probeBatch.addProbeArray(probeArray);
        probeBatch.commit();

        auto elapsedMilliSeconds = timer.elapsedMilliseconds();

        srand(static_cast<unsigned int>(time(0)));
        auto probeIndex = std::max<int>(0, (rand() % probeArray.numProbes()) - 1);
        float radius = probeArray.probes[probeIndex].influence.radius;
        listener = probeArray.probes[probeIndex].influence.center + ipl::Vector3f( radius / 2.0f, radius / 2.0f, radius / 2.0f);
        PrintOutput("%-25s: %10.2f %3s [%d triangles, %.2f spacing, %d probes]\n", "Probe Generation", elapsedMilliSeconds, "ms", numTriangles, spacing, probeBatch.numProbes());
    }


    // Bake reverb.
    auto openCL = nullptr;
    BakedDataIdentifier identifier;
    {
        identifier.variation = BakedDataVariation::Reverb;
        identifier.type = BakedDataType::Reflections;
        identifier.endpointInfluence = Sphere{};

        // Create simulator
        auto rays = 1024 * 16;
        auto diffuseSamples = 512;
        auto numSources = 1;
        auto numListeners = 1;
        auto threads = 12;
        auto bounces = 64;

        auto simulator = ReflectionSimulatorFactory::create(type, rays, diffuseSamples, duration, order, numSources, numListeners, threads, 1, radeonRays);

        Timer timer;
        timer.start();

        auto progress = [](float progress, void* userData) { PrintOutput("\r%-25s: %10.2f%% complete", "Reverb Bake", 100.0f * progress); };
        ReflectionBaker::bake(*scene, *simulator, identifier, true, false, rays, bounces, duration, duration, order, 1.0f, threads, 1, type, openCL, probeBatch, progress);
        probeBatch.commit();

        SerializedObject bakedDataSize;
        probeBatch.serializeAsRoot(bakedDataSize);

        auto elapsedSeconds = timer.elapsedSeconds();
        PrintOutput("\r%-10s %9.1f MB : %10.2f %3s [%d rays, %d bounces, %d threads]\n", "Reverb Bake", static_cast<float>(bakedDataSize.size()) / 1e6, elapsedSeconds, "s", rays, bounces, threads);
    }


    // baked data lookup.
    auto energyField = EnergyFieldFactory::create(type, duration, order, openCL);
    Reverb reverbTimes;
    {
        const int kMaxProbesPerNeighborhood = 8;

        ProbeNeighborhood probeNeighborhood;
        probeNeighborhood.resize(kMaxProbesPerNeighborhood);

        // Too much code at API level.
        ipl::unordered_set<const ProbeBatch*> probeLookup;
        probeLookup.reserve(16);

        Timer timer;
        timer.start();

        auto kNumLookupRuns = 100;
        for (auto run = 0; run < kNumLookupRuns; ++run)
        {
            probeNeighborhood.reset();
            probeBatch.getInfluencingProbes(listener, probeNeighborhood);
            probeNeighborhood.checkOcclusion(*scene, listener);
            probeNeighborhood.calcWeights(listener);

            BakedReflectionSimulator::findUniqueProbeBatches(probeNeighborhood, probeLookup);
            BakedReflectionSimulator::lookupEnergyField(identifier, probeNeighborhood, probeLookup, *energyField);
            BakedReflectionSimulator::lookupReverb(identifier, probeNeighborhood, probeLookup, reverbTimes);
        }

        auto elapsedTimeMicroSeconds = timer.elapsedMicroseconds() / kNumLookupRuns;
        PrintOutput("%-25s: %10.2f %3s [%d valid probes]\n", "Probe Lookup", elapsedTimeMicroSeconds, "us", probeNeighborhood.numValidProbes());
    }

    // IR Reconstruction
    auto convType = ipl::IndirectEffectType::Hybrid;
    auto sampleRate = 48000;
    auto ir = ImpulseResponseFactory::create(convType, duration, order, sampleRate, openCL);
    {
        auto reconstructor = ReconstructorFactory::create(type, convType, duration, order, sampleRate, radeonRays);

        ImpulseResponse* irs[1] = { ir.get() };
        EnergyField* fields[1] = { energyField.get() };
        float* distanceAttenuation[1] = { nullptr };
        AirAbsorptionModel airAbsorptionModel[1] = { AirAbsorptionModel{} };

        Timer timer;
        timer.start();

        auto kNumReconstructionRuns = 10;
        for (auto run = 0; run < kNumReconstructionRuns; ++run)
        {
            reconstructor->reconstruct(1, fields, distanceAttenuation, airAbsorptionModel, irs, ReconstructionType::Gaussian, duration, order);
        }

        auto elapsedTimeMilliSeconds = timer.elapsedMilliseconds() / kNumReconstructionRuns;
        PrintOutput("%-25s: %10.2f %3s [%.1f seconds, %d order, %d samplerate]\n", "IR Reconstruction", elapsedTimeMilliSeconds, "ms", duration, order, sampleRate);
    }

    // EQ Estimation
    float hybridEQ[Bands::kNumBands];
    int hybridDelay;
    {
        auto transitionTime = duration;
        auto overlapFraction = 0.25f;
        auto estimator = ipl::make_unique<HybridReverbEstimator>(duration, sampleRate, frameSize);

        Timer timer;
        timer.start();

        auto kNumEQEstimationRuns = 100;
        for (auto run = 0; run < kNumEQEstimationRuns; ++run)
        {
            estimator->estimate(energyField.get(), reverbTimes, *ir, transitionTime, overlapFraction, order, hybridEQ, hybridDelay);
        }

        auto elapsedTimeMilliSeconds = timer.elapsedMilliseconds() / kNumEQEstimationRuns;
        PrintOutput("%-25s: %10.2f %3s [%d sample frame]\n", "EQ Estimation", elapsedTimeMilliSeconds, "ms", frameSize);
    }

    //// audio processing - partitioning
    int numChannels = (order + 1) * (order + 1);
    int numSamples = static_cast<int>(ceilf(duration * sampleRate));
    
    TripleBuffer<OverlapSaveFIR> overlapSaveFIR;
    overlapSaveFIR.initBuffers(numChannels, numSamples, frameSize);
    {
        OverlapSavePartitioner partitioner(frameSize);

        Timer timer;
        timer.start();

        auto kNumPartitionRuns = 100;
        for (auto run = 0; run < kNumPartitionRuns; ++run)
        {
            partitioner.partition(*ir, numChannels, numSamples, *overlapSaveFIR.writeBuffer);
        }
        auto elapsedTimeMilliSeconds = timer.elapsedMilliseconds() / kNumPartitionRuns;

        overlapSaveFIR.commitWriteBuffer();
        PrintOutput("%-25s: %10.2f %3s [%d channels, %d samples]\n", "Audio Partitioning", elapsedTimeMilliSeconds, "ms", numChannels, numSamples);
    }

    // audio processing - rendering
    {
        AudioSettings audioSettings{};
        audioSettings.samplingRate = sampleRate;
        audioSettings.frameSize = frameSize;

        HybridReverbEffectSettings hybridSettings{};
        hybridSettings.numChannels = numChannels;
        hybridSettings.irSize = numSamples;

        HRTFSettings hrtfSettings{};
        HRTFDatabase hrtf(hrtfSettings, audioSettings.samplingRate, audioSettings.frameSize);

        AmbisonicsBinauralEffectSettings binauralSettings{};
        binauralSettings.maxOrder = order;
        binauralSettings.hrtf = &hrtf;

        auto ambisonicsBinauralEffect = make_unique<AmbisonicsBinauralEffect>(audioSettings, binauralSettings);
        auto hybridReverbEffect = make_unique<HybridReverbEffect>(audioSettings, hybridSettings);

        AudioBuffer mono(1, audioSettings.frameSize);
        mono.makeSilent();

        AudioBuffer ambisonics(numChannels, audioSettings.frameSize);
        ambisonics.makeSilent();

        AudioBuffer stereo(2, audioSettings.frameSize);
        stereo.makeSilent();

        HybridReverbEffectParams hybridParams{};
        hybridParams.numChannels = numChannels;
        hybridParams.numSamples = numSamples;
        hybridParams.reverb = &reverbTimes;
        hybridParams.eqCoeffs = hybridEQ;
        hybridParams.delay = hybridDelay;

        overlapSaveFIR.updateReadBuffer();
        hybridParams.fftIR = &overlapSaveFIR;

        AmbisonicsBinauralEffectParams binauralParams{};
        binauralParams.hrtf = &hrtf;
        binauralParams.order = order;

        Timer timer;
        timer.start();

        auto kNumReverbEffectRuns = 100;
        for (auto run = 0; run < kNumReverbEffectRuns; ++run)
        {
            hybridReverbEffect->apply(hybridParams, mono, ambisonics);
            ambisonicsBinauralEffect->apply(binauralParams, ambisonics, stereo);
        }

        auto elapsedTimeMicroSeconds = timer.elapsedMicroseconds() / kNumReverbEffectRuns;
        PrintOutput("%-25s: %10.2f %3s [%d channels, %d samples]\n", "Audio Rendering", elapsedTimeMicroSeconds, "us", numChannels, numSamples);
    }
}

BENCHMARK(hybridreverbsimulator)
{
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    PrintOutput("Running benchmark: Reflection Simulation (Embree)...\n");

    {
        auto irDuration = 2.0f;
        auto order = 1;
        auto frameSize = 512;
        PrintOutput("\n:: IR Duration (%.1f), Order (%d), Frame Size (%d), Bands (%d)\n", irDuration, order, frameSize, Bands::kNumBands);
        BenchmarkHybridReverbSimulatorForSettings("../../data/meshes/sponza.obj", SceneType::Embree, 2.0f, irDuration, order, frameSize);
    }

    {
        auto irDuration = 1.0f;
        auto order = 1;
        auto frameSize = 512;
        PrintOutput("\n:: IR Duration (%.1f), Order (%d), Frame Size (%d), Bands (%d)\n", irDuration, order, frameSize, Bands::kNumBands);
        BenchmarkHybridReverbSimulatorForSettings("../../data/meshes/sponza.obj", SceneType::Embree, 2.0f, irDuration, order, frameSize);
    }

    PrintOutput("\n");
#endif

}