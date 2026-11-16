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
#include <complex>
#include <utility>
#include <vector>

#include <air_absorption.h>
#include <energy_field.h>
#include <impulse_response.h>
#include <reconstructor.h>

#if !defined(IPL_ENABLE_OCTAVE_BANDS)

namespace {

using Complex = std::complex<double>;

void fft(std::vector<Complex>& a)
{
    const auto n = a.size();

    for (auto i = size_t{ 1 }, j = size_t{ 0 }; i < n; ++i)
    {
        auto bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (auto len = size_t{ 2 }; len <= n; len <<= 1)
    {
        const auto w = std::polar(1.0, -2.0 * 3.14159265358979323846 / static_cast<double>(len));
        for (auto i = size_t{ 0 }; i < n; i += len)
        {
            auto wk = Complex{ 1.0, 0.0 };
            for (auto k = size_t{ 0 }; k < len / 2; ++k, wk *= w)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * wk;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}

// Mean power per FFT bin in the third octave around centerFrequency.
double thirdOctaveLevel(const std::vector<Complex>& spectrum, int samplingRate, double centerFrequency)
{
    const auto n = spectrum.size();
    const auto low = centerFrequency * std::pow(2.0, -1.0 / 6.0);
    const auto high = centerFrequency * std::pow(2.0, 1.0 / 6.0);

    auto sum = 0.0;
    auto count = 0;
    for (auto k = size_t{ 0 }; k <= n / 2; ++k)
    {
        const auto frequency = static_cast<double>(k) * samplingRate / static_cast<double>(n);
        if (frequency >= low && frequency < high)
        {
            sum += std::norm(spectrum[k]);
            ++count;
        }
    }

    return sum / count;
}

double decibels(double power)
{
    return 10.0 * std::log10(power);
}

}

// If every band is reconstructed from the same noise, the band filters add it coherently and the response has a hole
// where two bands overlap (about 800 Hz and 8 kHz). With independent noise, a flat energy field gives a flat response.
TEST_CASE("A flat energy field is reconstructed with a flat spectrum.", "[Reconstructor]")
{
    const auto samplingRate = 48000;
    const auto duration = 4.0f;
    const auto order = 0;

    ipl::EnergyField energyField(duration, order);
    for (auto iBand = 0; iBand < ipl::Bands::kNumBands; ++iBand)
    {
        for (auto iBin = 0; iBin < energyField.numBins(); ++iBin)
        {
            energyField[0][iBand][iBin] = 1.0f;
        }
    }

    float noAbsorption[ipl::Bands::kNumBands] = {};
    ipl::AirAbsorptionModel airAbsorption(noAbsorption, nullptr, nullptr);

    ipl::ImpulseResponse impulseResponse(duration, order, samplingRate);

    ipl::Reconstructor reconstructor(duration, order, samplingRate);

    for (auto type : { ipl::ReconstructionType::Linear, ipl::ReconstructionType::Gaussian })
    {
        const ipl::EnergyField* energyFields[] = { &energyField };
        const float* distanceAttenuationCorrectionCurves[] = { nullptr };
        ipl::ImpulseResponse* impulseResponses[] = { &impulseResponse };

        reconstructor.reconstruct(1, energyFields, distanceAttenuationCorrectionCurves, &airAbsorption,
                                  impulseResponses, type, duration, order);

        std::vector<Complex> spectrum(262144, Complex{ 0.0, 0.0 });
        for (auto i = 0; i < impulseResponse.numSamples(); ++i)
        {
            spectrum[i] = impulseResponse[0][i];
        }
        fft(spectrum);

        // Reference level: well below the first band edge, where the first band is all there is.
        auto reference = 0.0;
        for (auto frequency : { 125.0, 157.0, 198.0, 250.0 })
        {
            reference += thirdOctaveLevel(spectrum, samplingRate, frequency) / 4.0;
        }

        // From 315 Hz up to 16 kHz, in third octaves.
        for (auto i = -5; i <= 12; ++i)
        {
            const auto frequency = 1000.0 * std::pow(2.0, i / 3.0);
            const auto level = decibels(thirdOctaveLevel(spectrum, samplingRate, frequency) / reference);

            INFO("third octave around " << frequency << " Hz");
            CHECK(std::fabs(level) < 1.5);
        }
    }
}

#endif
