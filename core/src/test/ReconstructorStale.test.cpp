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

#include <air_absorption.h>
#include <energy_field.h>
#include <impulse_response.h>
#include <reconstructor.h>

namespace {

// Energy of the omni channel of an impulse response from `start` seconds on.
double energyAfter(const ipl::ImpulseResponse& impulseResponse,
                   float start,
                   int samplingRate)
{
    auto sum = 0.0;
    for (auto i = static_cast<int>(start * samplingRate); i < impulseResponse.numSamples(); ++i)
    {
        sum += static_cast<double>(impulseResponse[0][i]) * impulseResponse[0][i];
    }

    return sum;
}

}

// The reconstructor writes the band responses in a buffer that it keeps between calls, and never clears it. Where it
// does not write (the bins of the energy field that are empty, or the samples after the end of a field that is shorter
// than the impulse response), the next reconstruction found what the previous one had left there.
TEST_CASE("The Gaussian reconstruction leaves nothing in the bins of an empty part of the field", "[Reconstructor]")
{
    const auto samplingRate = 48000;
    const auto duration = 0.5f;
    const auto order = 0;

    ipl::EnergyField fullField(duration, order);
    ipl::EnergyField sparseField(duration, order);
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        for (auto bin = 0; bin < fullField.numBins(); ++bin)
        {
            fullField[0][band][bin] = 1.0f;
            sparseField[0][band][bin] = (bin < 5) ? 1.0f : 0.0f;
        }
    }

    float noAbsorption[ipl::Bands::kNumBands] = {};
    ipl::AirAbsorptionModel airAbsorption(noAbsorption, nullptr, nullptr);

    ipl::ImpulseResponse impulseResponse(duration, order, samplingRate);
    ipl::Reconstructor reconstructor(duration, order, samplingRate);

    const float* distanceAttenuationCorrectionCurves[] = { nullptr };
    ipl::ImpulseResponse* impulseResponses[] = { &impulseResponse };

    const ipl::EnergyField* fullFields[] = { &fullField };
    reconstructor.reconstruct(1, fullFields, distanceAttenuationCorrectionCurves, &airAbsorption, impulseResponses,
                              ipl::ReconstructionType::Gaussian, duration, order);

    const ipl::EnergyField* sparseFields[] = { &sparseField };
    reconstructor.reconstruct(1, sparseFields, distanceAttenuationCorrectionCurves, &airAbsorption, impulseResponses,
                              ipl::ReconstructionType::Gaussian, duration, order);

    // The second field is empty from 50 ms on: so is the response from 100 ms on (the band filters ring for a while).
    const auto energy = energyAfter(impulseResponse, 0.1f, samplingRate);
    INFO("energy of the response after 100 ms: " << energy);
    CHECK(energy < 1e-6);
}

TEST_CASE("The linear reconstruction leaves nothing after the end of a field shorter than the response", "[Reconstructor]")
{
    const auto samplingRate = 48000;
    const auto duration = 0.5f;
    const auto order = 0;

    ipl::EnergyField longField(duration, order);
    ipl::EnergyField shortField(0.2f, order);
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        for (auto bin = 0; bin < longField.numBins(); ++bin)
        {
            longField[0][band][bin] = 1.0f;
        }

        for (auto bin = 0; bin < shortField.numBins(); ++bin)
        {
            shortField[0][band][bin] = 1.0f;
        }
    }

    float noAbsorption[ipl::Bands::kNumBands] = {};
    ipl::AirAbsorptionModel airAbsorption(noAbsorption, nullptr, nullptr);

    ipl::ImpulseResponse impulseResponse(duration, order, samplingRate);
    ipl::Reconstructor reconstructor(duration, order, samplingRate);

    const float* distanceAttenuationCorrectionCurves[] = { nullptr };
    ipl::ImpulseResponse* impulseResponses[] = { &impulseResponse };

    const ipl::EnergyField* longFields[] = { &longField };
    reconstructor.reconstruct(1, longFields, distanceAttenuationCorrectionCurves, &airAbsorption, impulseResponses,
                              ipl::ReconstructionType::Linear, duration, order);

    const ipl::EnergyField* shortFields[] = { &shortField };
    reconstructor.reconstruct(1, shortFields, distanceAttenuationCorrectionCurves, &airAbsorption, impulseResponses,
                              ipl::ReconstructionType::Linear, duration, order);

    // The second field ends at 200 ms: so does the response, up to the ringing of the band filters.
    const auto energy = energyAfter(impulseResponse, 0.25f, samplingRate);
    INFO("energy of the response after 250 ms: " << energy);
    CHECK(energy < 1e-6);
}
