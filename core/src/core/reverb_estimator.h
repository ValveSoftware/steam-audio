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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Reverb
// --------------------------------------------------------------------------------------------------------------------

struct Reverb
{
    float reverbTimes[Bands::kNumBands]; // Time taken for the energy to decay by 60 dB, for each band.
};


// --------------------------------------------------------------------------------------------------------------------
// I3DL2Reverb
// --------------------------------------------------------------------------------------------------------------------

// An I3DL2-compliant parametric reverb.
struct I3DL2Reverb
{
    float room; // Total energy in the early and late portions, in mB.
    float roomHigh; // Total energy in the early and late portions for high frequencies, in mB.
    float roomLow; // Total energy in the early and late portions for low frequencies, in mB.
    float decayTime; // Time taken for the energy to decay by 60 dB.
    float decayHighRatio; // Ratio between decay time at high frequency and decay time at mid frequency.
    float reflections; // Total energy in the early portion, in mB.
    float reflectionsDelay; // Delay between direct sound and first early reflection.
    float reverb; // Total energy in the late portion, in mB.
    float reverbDelay; // Delay between first early reflection and first late reflection.
    float hfReference; // Cutoff for high frequencies.
    float lfReference; // Cutoff for low frequencies.
    float roomRolloff; // Distance-based attenuation for the room reverb effect. Ignored.
    float diffusion; // Number of echoes per unit time, as a percentage.
    float density; // Number of room modes per unit frequency, as a percentage.
};


// --------------------------------------------------------------------------------------------------------------------
// ReverbEstimator
// --------------------------------------------------------------------------------------------------------------------

// Estimates a parametric reverb based on an energy field.
class ReverbEstimator
{
public:
    // Estimates a parametric reverb.
    static void estimate(const EnergyField& energyField,
                         const AirAbsorptionModel& airAbsorption,
                         I3DL2Reverb& reverb);

    static void estimate(const EnergyField& energyField,
                         const AirAbsorptionModel& airAbsorption,
                         Reverb& reverb);

    static void applyReverbScale(const float* reverbScale,
                                 EnergyField& energyField);

private:
    static const float kEarlyReflectionsDuration;
    static const float kMinEnergyForLineFit;
    static const float kMaxEnergyForLineFit;
    static const float kDiffusionEnergyThreshold;

    // Calculates the total energy in a histogram within a specified interval of time.
    static float totalEnergyInHistogramRange(float startTime,
                                             float endTime,
                                             const float* histogram);

    // Calculates the total energy in a histogram.
    static float totalEnergyInHistogram(const float* histogram,
                                        int numBins);

    // Calculates the time at which energy first arrives in a histogram, after a given start time.
    static float firstArrivalAfter(float startTime,
                                   const float* histogram,
                                   int numBins);

    // Calculates the reverb time based on a histogram.
    static float reverbTime(const float* histogram,
                            int numBins,
                            const AirAbsorptionModel& airAbsorption,
                            int band);

    // Calculates the echo diffusion based on a histogram.
    static float diffusion(const float* histogram,
                           int numBins,
                           float startTime);

    // Calculates the modal density based on a histogram.
    static float modalDensity(const float* const* echogram,
                              int numBins);

    // Converts all linear-scale amplitudes to log-scale millibel (mB) values.
    static void convertUnits(I3DL2Reverb& parameters);

    // Clamps all parameters to within the I3DL2-specified valid ranges.
    static void clampToValidRanges(I3DL2Reverb& parameters);
};

}
