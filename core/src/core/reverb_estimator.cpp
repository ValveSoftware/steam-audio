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

#include "reverb_estimator.h"

#include "array_math.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ReverbEstimator
// --------------------------------------------------------------------------------------------------------------------

const float ReverbEstimator::kEarlyReflectionsDuration = 0.08f;
const float ReverbEstimator::kMinEnergyForLineFit = -2.5f;
const float ReverbEstimator::kMaxEnergyForLineFit = -0.5f;
const float ReverbEstimator::kDiffusionEnergyThreshold = 1e-7f;

void ReverbEstimator::estimate(const EnergyField& energyField,
                               const AirAbsorptionModel& airAbsorption,
                               I3DL2Reverb& reverb)
{
    reverb.roomRolloff = 0.0f;
    reverb.lfReference = 250.0f;
    reverb.hfReference = 5000.0f;

    reverb.room = totalEnergyInHistogram(energyField[0][1], energyField.numBins());
    reverb.roomLow = totalEnergyInHistogram(energyField[0][0], energyField.numBins());
    reverb.roomHigh = totalEnergyInHistogram(energyField[0][2], energyField.numBins());

    reverb.reflectionsDelay = firstArrivalAfter(0.0f, energyField[0][1], energyField.numBins());
    reverb.reverbDelay = firstArrivalAfter(reverb.reflectionsDelay + kEarlyReflectionsDuration, energyField[0][1], energyField.numBins()) - reverb.reflectionsDelay;

    reverb.reflections = totalEnergyInHistogramRange(0.0f, reverb.reflectionsDelay + kEarlyReflectionsDuration, energyField[0][1]) / reverb.room;
    reverb.reverb = 1.0f - reverb.reflections;

    reverb.decayTime = reverbTime(energyField[0][1], energyField.numBins(), airAbsorption, 1);
    reverb.decayHighRatio = reverbTime(energyField[0][2], energyField.numBins(), airAbsorption, 2) / reverb.decayTime;

    reverb.diffusion = diffusion(energyField[0][1], energyField.numBins(), reverb.reflectionsDelay + reverb.reverbDelay);
    reverb.density = modalDensity(energyField[0], energyField.numBins());

    convertUnits(reverb);
    clampToValidRanges(reverb);
}

void ReverbEstimator::estimate(const EnergyField& energyField,
                               const AirAbsorptionModel& airAbsorption,
                               Reverb& reverb)
{
    for (int i = 0; i < Bands::kNumBands; ++i)
    {
        reverb.reverbTimes[i] = std::max(reverbTime(energyField[0][i], energyField.numBins(), airAbsorption, i), 0.1f);
    }
}

void ReverbEstimator::applyReverbScale(const float* reverbScale,
                                       EnergyField& energyField)
{
    PROFILE_FUNCTION();

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        auto energy = energyField[0][i];
        auto reverbTimeRatio = reverbScale[i];

        auto peakBin = 0;
        auto peakEnergy = 0.0f;
        ArrayMath::maxIndex(energyField.numBins(), energy, peakEnergy, peakBin);

        auto oldTotalEnergy = 0.0f;
        for (auto j = 0; j < energyField.numBins(); ++j)
        {
            oldTotalEnergy += energyField[0][i][j];
        }

        for (auto j = peakBin; j < energyField.numBins(); ++j)
        {
            auto oldEnergy = energy[j];
            if (oldEnergy <= 0.0f)
                continue;

            auto newEnergy = peakEnergy * powf(oldEnergy / peakEnergy, 1.0f / reverbTimeRatio);
            auto scalar = newEnergy / oldEnergy;

            for (auto k = 0; k < energyField.numChannels(); ++k)
            {
                energyField[k][i][j] *= scalar;
            }
        }

        auto newTotalEnergy = 0.0f;
        for (auto j = 0; j < energyField.numBins(); ++j)
        {
            newTotalEnergy += energyField[0][i][j];
        }

        auto energyScalar = (newTotalEnergy > 0.0f) ? oldTotalEnergy / newTotalEnergy : 1.0f;
        for (auto j = 0; j < energyField.numBins(); ++j)
        {
            for (auto k = 0; k < energyField.numChannels(); ++k)
            {
                energyField[k][i][j] *= energyScalar;
            }
        }
    }
}

float ReverbEstimator::totalEnergyInHistogramRange(float startTime,
                                                   float endTime,
                                                   const float* histogram)
{
    auto startBin = static_cast<int>(floorf(startTime / EnergyField::kBinDuration));
    auto endBin = static_cast<int>(floorf(endTime / EnergyField::kBinDuration));

    auto totalEnergy = 0.0f;
    for (auto i = startBin; i <= endBin; ++i)
    {
        totalEnergy += histogram[i];
    }

    return totalEnergy;
}

float ReverbEstimator::totalEnergyInHistogram(const float* histogram,
                                              int numBins)
{
    return totalEnergyInHistogramRange(0.0f, numBins * EnergyField::kBinDuration, histogram);
}

float ReverbEstimator::firstArrivalAfter(float startTime,
                                         const float* histogram,
                                         int numBins)
{
    auto firstArrivalTime = startTime;
    auto firstArrivalBin = static_cast<int>(floorf(firstArrivalTime / EnergyField::kBinDuration));

    for (auto i = firstArrivalBin; i < numBins; ++i, firstArrivalTime += EnergyField::kBinDuration)
    {
        if (histogram[i] > 0.0f)
            break;
    }

    return firstArrivalTime;
}

float ReverbEstimator::reverbTime(const float* histogram,
                                  int numBins,
                                  const AirAbsorptionModel& airAbsorption,
                                  int band)
{
    // Calculate the total energy in the histogram, weighted by air absorption.
    auto totalEnergy = 0.0f;
    auto x = 0.0f;
    for (auto i = 0; i < numBins; ++i)
    {
        totalEnergy += histogram[i] * airAbsorption.evaluate(x, band);
        x += EnergyField::kBinDuration;
    }

    if (totalEnergy < 1e-4f)
        return 0.0f;

    auto energy = 0.0f;
    auto sumX = 0.0f;
    auto sumY = 0.0f;
    auto sumXX = 0.0f;
    auto sumXY = 0.0f;
    auto N = 0;

    // Now calculate the Energy Decay Curve (EDC). The EDC is defined by EDC[i] = EDC[i + 1] + E[i],
    // where E[i] is the energy in bin i. All energies are weighted by air absorption. We calculate
    // the EDC by looping backwards from the end of the histogram. We also normalize the EDC and
    // convert it to log-scale in order to fit a line to the EDC.
    for (auto i = numBins - 1; i >= 0; --i)
    {
        energy += histogram[i] * airAbsorption.evaluate(x, band);
        auto y = log10f(energy / totalEnergy);

        // Use least-squares fitting to fit a line to the EDC. We only consider points on the EDC
        // within a certain range of energies.
        if (kMinEnergyForLineFit <= y && y <= kMaxEnergyForLineFit)
        {
            sumX += x;
            sumY += y;
            sumXX += x * x;
            sumXY += x * y;
            ++N;
        }

        x -= EnergyField::kBinDuration;
    }

    // Calculate the slope of the line.
    auto numerator = (N * sumXY) - (sumX * sumY);
    auto denominator = (N * sumXX) - (sumX * sumX);

    // Calculate the reverb time as a function of the slope of the line.
    return (fabsf(numerator) > std::numeric_limits<float>::min()) ? std::max(0.0f, -6.0f * (denominator / numerator)) : 0.0f;
}

float ReverbEstimator::diffusion(const float* histogram,
                                 int numBins,
                                 float startTime)
{
    auto startBin = static_cast<int>(floorf(startTime / EnergyField::kBinDuration));
    auto endBin = numBins - 1;

    // Find the last non-zero bin in the histogram.
    for (auto i = numBins - 1; i >= startBin; --i)
    {
        if (histogram[i] > 0.0f)
        {
            endBin = i;
            break;
        }
    }

    // If not enough bins have non-zero values, just set the diffusion to 100%.
    if (startBin == endBin)
        return 100.0f;

    // Now define the diffusion as the percentage of bins between startBin and endBin
    // whose energy values are above a threshold.
    auto numBinsAboveThreshold = 0;
    for (auto i = startBin; i <= endBin; ++i)
    {
        if (histogram[i] >= kDiffusionEnergyThreshold)
        {
            numBinsAboveThreshold++;
        }
    }

    return 100.0f * (numBinsAboveThreshold / (endBin - startBin + 1.0f));
}

float ReverbEstimator::modalDensity(const float* const* echogram,
                                    int numBins)
{
    auto lowFreqEnergy = 0.0f;
    auto midFreqEnergy = 0.0f;
    auto highFreqEnergy = 0.0f;

    // Calculate the total energy in each band of the echogram.
    for (auto i = 0; i < numBins; ++i)
    {
        lowFreqEnergy += echogram[0][i];
        midFreqEnergy += echogram[1][i];
        highFreqEnergy += echogram[2][i];
    }

    // Find the difference in energy between the band with the most energy and the band with the least energy.
    auto minEnergy = std::min({ lowFreqEnergy, midFreqEnergy, highFreqEnergy });
    auto maxEnergy = std::max({ lowFreqEnergy, midFreqEnergy, highFreqEnergy });

    // If there's nearly zero energy in the echogram, use a modal density of 100%. Otherwise, define
    // the modal density as the ratio of the difference between the minimum and maximum energy to the maximum
    // energy (as a percentage).
    return (maxEnergy < std::numeric_limits<float>::min()) ? 100.0f : 100.0f * (1.0f - ((maxEnergy - minEnergy) / maxEnergy));
}

void ReverbEstimator::convertUnits(I3DL2Reverb& parameters)
{
    parameters.room = 1000.0f * log10f(parameters.room);
    parameters.roomLow = 1000.0f * log10f(parameters.roomLow);
    parameters.roomHigh = 1000.0f * log10f(parameters.roomHigh);
    parameters.reflections = 1000.0f * log10f(parameters.reflections);
    parameters.reverb = 1000.0f * log10f(parameters.reverb);
}

void ReverbEstimator::clampToValidRanges(I3DL2Reverb& parameters)
{
    parameters.room = std::min(std::max(-10000.0f, parameters.room), 0.0f);
    parameters.roomHigh = std::min(std::max(-10000.0f, parameters.roomHigh), 0.0f);
    parameters.roomLow = std::min(std::max(-10000.0f, parameters.roomLow), 0.0f);
    parameters.decayTime = std::min(std::max(0.1f, parameters.decayTime), 20.0f);
    parameters.decayHighRatio = std::min(std::max(0.1f, parameters.decayHighRatio), 2.0f);
    parameters.reflections = std::min(std::max(-10000.0f, parameters.reflections), 1000.0f);
    parameters.reflectionsDelay = std::min(std::max(0.0f, parameters.reflectionsDelay), 0.3f);
    parameters.reverb = std::min(std::max(-10000.0f, parameters.reverb), 2000.0f);
    parameters.reverbDelay = std::min(std::max(0.0f, parameters.reverbDelay), 0.1f);
    parameters.diffusion = std::min(std::max(0.0f, parameters.diffusion), 100.0f);
    parameters.density = std::min(std::max(0.0f, parameters.density), 100.0f);
}

}
