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
#include "energy_field.h"
#include "iir.h"
#include "impulse_response.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IReconstructor
// --------------------------------------------------------------------------------------------------------------------

enum class ReconstructionType
{
    Gaussian,
    Linear
};

// Represents the state necessary to reconstruct an Ambisonics impulse response from an Ambisonics energy field.
class IReconstructor
{
public:
    virtual ~IReconstructor()
    {}

    virtual void reconstruct(int numIRs,
                             const EnergyField* const* energyFields,
                             const float* const* distanceAttenuationCorrectionCurves,
                             const AirAbsorptionModel* airAbsorptionModels,
                             ImpulseResponse* const* impulseResponses,
                             ReconstructionType type,
                             float duration,
                             int order) = 0;

protected:
    static const float kEnergyThreshold;
    static const float kMinVariance;
};


// --------------------------------------------------------------------------------------------------------------------
// Reconstructor
// --------------------------------------------------------------------------------------------------------------------

// A CPU implementation of IReconstructor.
class Reconstructor : public IReconstructor
{
public:
    Reconstructor(float maxDuration,
                  int maxOrder,
                  int samplingRate);

    virtual void reconstruct(int numIRs,
                             const EnergyField* const* energyFields,
                             const float* const* distanceAttenuationCorrectionCurves,
                             const AirAbsorptionModel* airAbsorptionModels,
                             ImpulseResponse* const* impulseResponses,
                             ReconstructionType type,
                             float duration,
                             int order) override;

private:
    float mMaxDuration;
    int mMaxOrder;
    int mSamplingRate;
    Array<float, 2> mWhiteNoise;
    Array<float, 2> mBandIRs;
    Array<IIRFilterer> mFilters;
};

}
