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

#include <air_absorption.h>
#include <energy_field.h>
#include <impulse_response.h>
#include <reconstructor.h>
#include <sh.h>

namespace {

// Reconstructs the omni channel of an impulse response from an energy field in which every band holds `energy` (the
// energy of the reflections that reach the listener in one bin, as the reflection simulator accumulates it, in units
// where a source of power 1 has an intensity of 1 / (4 pi d^2)) in each of the first `numBins` bins.
std::vector<float> reconstructFlatField(int samplingRate,
                                        float energy,
                                        int numBins)
{
    const auto duration = 1.0f;
    const auto order = 0;

    ipl::EnergyField energyField(duration, order);
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        for (auto bin = 0; bin < numBins; ++bin)
        {
            // The simulator accumulates the omni coefficient, which is Y00 times the energy.
            energyField[0][band][bin] = ipl::SphericalHarmonics::evaluate(0, 0, ipl::Vector3f{}) * energy;
        }
    }

    float noAbsorption[ipl::Bands::kNumBands] = {};
    ipl::AirAbsorptionModel airAbsorption(noAbsorption, nullptr, nullptr);

    ipl::ImpulseResponse impulseResponse(duration, order, samplingRate);
    ipl::Reconstructor reconstructor(duration, order, samplingRate);

    const ipl::EnergyField* energyFields[] = { &energyField };
    const float* distanceAttenuationCorrectionCurves[] = { nullptr };
    ipl::ImpulseResponse* impulseResponses[] = { &impulseResponse };
    reconstructor.reconstruct(1, energyFields, distanceAttenuationCorrectionCurves, &airAbsorption, impulseResponses,
                              ipl::ReconstructionType::Linear, duration, order);

    return std::vector<float>(impulseResponse[0], impulseResponse[0] + impulseResponse.numSamples());
}

}

// A direct path to a listener at distance d has an impulse response of 1 / d: an energy of 1 / d^2 (omni channel / Y00).
// The reflection simulator's energy field is in intensity units, 1 / (4 pi d^2) for the same source. A reflection, like
// the direct sound, has to give back its energy: the energy of the impulse response (a sum over its samples) is 4 pi
// times that of the energy field, whatever the sampling rate. It was 5 to 13 dB too high (9.5 dB at 48 kHz), and
// proportional to the sampling rate, because the noise of a bin has the energy of the bin in every one of its samples.
TEST_CASE("The impulse response has the energy of the energy field", "[Reconstructor]")
{
    const auto energy = 1e-3f;
    const auto numBins = 50;
    const auto omni = ipl::SphericalHarmonics::evaluate(0, 0, ipl::Vector3f{});

    for (auto samplingRate : { 24000, 44100, 48000, 96000 })
    {
        const auto ir = reconstructFlatField(samplingRate, energy, numBins);

        // The first and last bins are half bins (the amplitude is interpolated from and to nothing): leave them out.
        const auto numSamplesPerBin = static_cast<int>(std::ceil(ipl::EnergyField::kBinDuration * samplingRate));
        const auto start = numSamplesPerBin;
        const auto end = (numBins - 1) * numSamplesPerBin;

        auto sum = 0.0;
        for (auto i = start; i < end; ++i)
        {
            sum += static_cast<double>(ir[i]) * ir[i];
        }

        const auto numBinsInSum = (numBins - 2);
        const auto expected = 4.0 * 3.14159265358979 * energy * numBinsInSum;
        const auto measured = sum / (static_cast<double>(omni) * omni);

        INFO("sampling rate " << samplingRate << " Hz");
        INFO("energy of the impulse response " << measured << ", energy of the energy field times 4 pi " << expected);
        // Every band is currently reconstructed from the same noise sequence, so the band filters add up coherently and
        // dip where adjacent bands overlap: the energy comes out 2.7 dB (24 kHz) to 0.8 dB (96 kHz) under the energy
        // field, 1.6 dB at 48 kHz. With independent noise per band the error is under 0.6 dB. Before the change it was
        // between +5 and +13 dB, so 3 dB tells the two apart.
        CHECK(std::fabs(10.0 * std::log10(measured / expected)) < 3.0);
    }
}
