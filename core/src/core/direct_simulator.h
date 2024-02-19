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

#include "air_absorption.h"
#include "coordinate_space.h"
#include "directivity.h"
#include "distance_attenuation.h"
#include "propagation_medium.h"
#include "sampling.h"
#include "scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DirectSimulator
// --------------------------------------------------------------------------------------------------------------------

enum DirectSimulationFlags
{
    CalcDistanceAttenuation = 1 << 0,
    CalcAirAbsorption = 1 << 1,
    CalcDirectivity = 1 << 2,
    CalcOcclusion = 1 << 3,
    CalcTransmission = 1 << 4,
    CalcDelay = 1 << 5
};

enum class OcclusionType
{
    Raycast,
    Volumetric
};

// Describes the properties of a direct sound path.
struct DirectSoundPath
{
    float distanceAttenuation;
    float airAbsorption[Bands::kNumBands];
    float delay;
    float occlusion;
    float transmission[Bands::kNumBands];
    float directivity;
};

// Encapsulates the state required to simulate direct sound, including distance attenuation, air absorption,
// partial occlusion, and propagation delays.
class DirectSimulator
{
public:
    DirectSimulator(int maxNumOcclusionSamples);

    void simulate(const IScene* scene,
                  DirectSimulationFlags flags,
                  const CoordinateSpace3f& source,
                  const CoordinateSpace3f& listener,
                  const DistanceAttenuationModel& distanceAttenuationModel,
                  const AirAbsorptionModel& airAbsorptionModel,
                  const Directivity& directivity,
                  OcclusionType occlusionType,
                  float occlusionRadius,
                  int numOcclusionSamples,
                  int numTransmissionRays,
                  DirectSoundPath& directSoundPath);

    static float directPathDelay(const Vector3f& listener,
                                 const Vector3f& source);

private:
    Array<Vector3f> mSphereVolumeSamples;

    float raycastOcclusion(const IScene& scene,
                           const Vector3f& listenerPosition,
                           const Vector3f& sourcePosition);

    float volumetricOcclusion(const IScene& scene,
                              const Vector3f& listenerPosition,
                              const Vector3f& sourcePosition,
                              float sourceRadius,
                              int numSamples);

    void transmission(const IScene& scene,
                      const Vector3f& listenerPosition,
                      const Vector3f& sourcePosition,
                      float* transmissionFactors,
                      int numTransmissionRays);
};

}
