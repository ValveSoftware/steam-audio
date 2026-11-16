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
#include <iomanip>
#include <vector>

#include <audio_buffer.h>
#include <iir.h>
#include <reverb_effect.h>

namespace {

// The response of the parametric reverb to a unit impulse.
std::vector<float> impulseResponse(const float* reverbTimes,
                                   int samplingRate,
                                   int frameSize,
                                   double duration)
{
    ipl::ReverbEffect effect(ipl::AudioSettings{ samplingRate, frameSize });

    ipl::Reverb reverb;
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        reverb.reverbTimes[band] = reverbTimes[band];
    }

    ipl::ReverbEffectParams params{};
    params.reverb = &reverb;

    ipl::AudioBuffer in(1, frameSize);
    ipl::AudioBuffer out(1, frameSize);

    const auto numFrames = static_cast<int>(duration * samplingRate) / frameSize;
    std::vector<float> response(numFrames * frameSize);
    for (auto frame = 0; frame < numFrames; ++frame)
    {
        in.makeSilent();
        if (frame == 0)
        {
            in[0][0] = 1.0f;
        }

        effect.apply(params, in, out);

        for (auto i = 0; i < frameSize; ++i)
        {
            response[frame * frameSize + i] = out[0][i];
        }
    }

    return response;
}

// The time that the energy of a band of a response takes to decay by 60 dB, from a line fitted to its Schroeder
// backward integral between -5 and -25 dB (a T20, as in ISO 3382).
double reverbTime(const std::vector<float>& response,
                  int band,
                  int samplingRate)
{
    ipl::IIR filters[ipl::Bands::kNumBands];
    ipl::IIR::bandFilters(filters, samplingRate);

    ipl::IIRFilterer filterer;
    filterer.setFilter(filters[band]);

    std::vector<float> bandResponse(response.size());
    filterer.apply(static_cast<int>(response.size()), response.data(), bandResponse.data());

    std::vector<double> edc(response.size());
    auto sum = 0.0;
    for (auto i = static_cast<int>(response.size()) - 1; i >= 0; --i)
    {
        sum += static_cast<double>(bandResponse[i]) * bandResponse[i];
        edc[i] = sum;
    }

    auto n = 0.0, sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
    for (size_t i = 0; i < edc.size(); ++i)
    {
        const auto level = 10.0 * std::log10(edc[i] / edc[0]);
        if (level <= -5.0 && level >= -25.0)
        {
            const auto x = static_cast<double>(i) / samplingRate;
            n += 1.0;
            sumX += x;
            sumY += level;
            sumXX += x * x;
            sumXY += x * level;
        }
    }

    const auto slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
    return -60.0 / slope;
}

}

// The feedback delay network is built so that its energy decays by 60 dB in the reverb time of each band. Each delay
// line has the gain that makes the energy decay by 60 dB over reverbTime seconds, for the length that the line has.
TEST_CASE("The parametric reverb decays in the requested reverb time", "[ReverbEffect]")
{
    const struct { int samplingRate; int frameSize; float reverbTime; } cases[] = {
        { 48000, 256, 1.5f },
        { 48000, 1024, 1.5f },
        { 44100, 1024, 1.5f },
        { 48000, 1024, 0.6f },
        { 48000, 1024, 3.0f },
    };

    for (const auto& c : cases)
    {
        const float reverbTimes[ipl::Bands::kNumBands] = { c.reverbTime, c.reverbTime, c.reverbTime };
        const auto response = impulseResponse(reverbTimes, c.samplingRate, c.frameSize, 4.0 * c.reverbTime + 1.0);

        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            const auto measured = reverbTime(response, band, c.samplingRate);

            INFO("sampling rate " << c.samplingRate << ", frame size " << c.frameSize << ", reverb time " << c.reverbTime << " s, band " << band << ": measured " << std::fixed << std::setprecision(3) << measured << " s");
            CHECK(std::fabs(measured / c.reverbTime - 1.0) < 0.15);
        }
    }
}
