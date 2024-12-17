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

#pragma once

#include "baked_reflection_data.h"
#include "direct_effect.h"
#include "hybrid_reverb_effect.h"
#include "opencl_device.h"
#include "simulation_manager.h"
#include "tan_convolution_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SimulationData
// --------------------------------------------------------------------------------------------------------------------

struct DirectSimulationInputs
{
    DirectSimulationFlags flags;
    CoordinateSpace3f source;
    DistanceAttenuationModel distanceAttenuationModel;
    AirAbsorptionModel airAbsorptionModel;
    Directivity directivity;
    OcclusionType occlusionType;
    float occlusionRadius;
    int numOcclusionSamples;
    int numTransmissionRays;
};

struct DirectSimulationOutputs
{
    DirectSoundPath directPath;
};

struct ReflectionSimulationInputs
{
    bool enabled;
    CoordinateSpace3f source;
    DistanceAttenuationModel distanceAttenuationModel;
    AirAbsorptionModel airAbsorptionModel;
    Directivity directivity;
    float reverbScale[Bands::kNumBands];
    float transitionTime;
    float overlapFraction;
    bool baked;
    BakedDataIdentifier bakedDataIdentifier;
};

struct ReflectionSimulationState
{
    CoordinateSpace3f prevSource;
    DistanceAttenuationModel prevDistanceAttenuationModel;
    Directivity prevDirectivity;
    unique_ptr<EnergyField> energyField;
    unique_ptr<EnergyField> accumEnergyField;
    int numFramesAccumulated;
    Array<float> distanceAttenuationCorrectionCurve;
    bool applyDistanceAttenuationCorrectionCurve;
    unique_ptr<ImpulseResponse> impulseResponse;
    unique_ptr<ImpulseResponse> impulseResponseCopy;
    std::atomic<bool> impulseResponseUpdated;
    bool validSimulationData;
};

struct ReflectionSimulationOutputs
{
    TripleBuffer<OverlapSaveFIR> overlapSaveFIR;
    Reverb reverb;
    float hybridEQ[Bands::kNumBands];
    int hybridDelay;
    int numChannels;
    int numSamples;
    shared_ptr<TANDevice> tan;
    int tanSlot;
};

struct PathingSimulationInputs
{
    bool enabled;
    CoordinateSpace3f source;
    shared_ptr<ProbeBatch> probes;
    float visRadius;
    float visThreshold;
    float visRange;
    int order;
    bool enableValidation;
    bool findAlternatePaths;
    bool simplifyPaths;
    bool realTimeVis;
};

struct PathingSimulationState
{
    float eq[Bands::kNumBands];
    Array<float> sh;
    Vector3f direction;
    float distanceRatio;
};

struct PathingSimulationOutputs
{
    float eq[Bands::kNumBands];
    Array<float> sh;
    Vector3f direction;
    float distanceRatio;
};

class SimulationData
{
public:
    DirectSimulationInputs directInputs;
    ReflectionSimulationInputs reflectionInputs;
    PathingSimulationInputs pathingInputs;

    DirectSimulationOutputs directOutputs;
    ReflectionSimulationOutputs reflectionOutputs;
    PathingSimulationOutputs pathingOutputs;

    ReflectionSimulationState reflectionState;
    PathingSimulationState pathingState;


    SimulationData(bool enableIndirect,
                   bool enablePathing,
                   SceneType sceneType,
                   IndirectEffectType indirectType,
                   int maxNumOcclusionSamples,
                   float maxDuration,
                   int maxOrder,
                   int samplingRate,
                   int frameSize,
                   shared_ptr<OpenCLDevice> openCL,
                   shared_ptr<TANDevice> tan);

    ~SimulationData();

    bool hasSourceChanged() const;
};

}
