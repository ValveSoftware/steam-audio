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
#include <cstdlib>
#include <iomanip>
#include <vector>

#include <audio_buffer.h>
#include <iir.h>
#include <reverb_effect.h>

namespace {

const auto kSamplingRate = 48000;
const auto kFrameSize = 256;

// The delay lines of the reverb are drawn with rand(): seed it, so that the result does not depend on what was drawn
// before. Delay lines are chosen among the powers of primes, so there are only a few different draws.
std::vector<float> impulseResponse(const float* reverbTimes,
                                   double duration,
                                   unsigned int seed)
{
    srand(seed);

    ipl::ReverbEffect effect(ipl::AudioSettings{ kSamplingRate, kFrameSize });

    ipl::Reverb reverb;
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        reverb.reverbTimes[band] = reverbTimes[band];
    }

    ipl::ReverbEffectParams params{};
    params.reverb = &reverb;

    ipl::AudioBuffer in(1, kFrameSize);
    ipl::AudioBuffer out(1, kFrameSize);

    const auto numFrames = static_cast<int>(duration * kSamplingRate) / kFrameSize;
    std::vector<float> response(numFrames * kFrameSize);
    for (auto frame = 0; frame < numFrames; ++frame)
    {
        in.makeSilent();
        if (frame == 0)
        {
            in[0][0] = 1.0f;
        }

        effect.apply(params, in, out);

        for (auto i = 0; i < kFrameSize; ++i)
        {
            response[frame * kFrameSize + i] = out[0][i];
        }
    }

    return response;
}

// The time that the energy around a frequency (in a third of an octave) takes to decay by 60 dB, from a line fitted to
// the Schroeder backward integral of the response between -5 and -25 dB.
double reverbTimeAround(const std::vector<float>& response,
                        double frequency)
{
    const auto lowCutoff = static_cast<float>(frequency / std::pow(2.0, 1.0 / 6.0));
    const auto highCutoff = static_cast<float>(frequency * std::pow(2.0, 1.0 / 6.0));

    ipl::IIRFilterer filterer;
    filterer.setFilter(ipl::IIR::bandPass(lowCutoff, highCutoff, kSamplingRate));

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
            const auto x = static_cast<double>(i) / kSamplingRate;
            n += 1.0;
            sumX += x;
            sumY += level;
            sumXX += x * x;
            sumXY += x * level;
        }
    }

    return -60.0 / ((n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX));
}

}

// With the same reverb time in every band, the reverb has to decay at that reverb time at every frequency. The three
// filters that absorb the energy in a delay line (a low shelf, a peaking filter and a high shelf) overlap, so their
// product was not flat: the decay was 8% faster around 400 Hz, and 18% slower around 5 kHz.
//
// The network decays a little more slowly than its reverb time even without any filter (up to 7% in a third of an
// octave over the draws of the delay lines that were tried, 80 of them: 2 to 7% above 1.5 s), so the limit is 10%, and the
// filters that were replaced were up to 18% off.
TEST_CASE("The parametric reverb decays at the same rate at every frequency when the reverb times are equal", "[ReverbEffect]")
{
    const float reverbTimes[ipl::Bands::kNumBands] = { 1.5f, 1.5f, 1.5f };

    for (const auto seed : { 1u, 2u })
    {
        const auto response = impulseResponse(reverbTimes, 7.0, seed);

        for (const auto frequency : { 100.0, 400.0, 1250.0, 2500.0, 5000.0, 12500.0 })
        {
            const auto measured = reverbTimeAround(response, frequency);
            INFO("draw " << seed << ", around " << frequency << " Hz: measured " << std::fixed << std::setprecision(3) << measured << " s");
            CHECK(std::fabs(measured / 1.5 - 1.0) < 0.10);
        }
    }
}

// With different reverb times, the reverb time has to be that of the band at the center of the band, and in between
// it has to go from the one to the other. Each time is compared with the one that the same delay lines give with the
// same reverb time in every band, so that what the network does on its own (see above) does not count: the ratio is
// within 5.1% of the ratio of the reverb times for every one of 40 draws that were tried, and 8% is
// the limit.
TEST_CASE("The parametric reverb decays at the reverb time of each band", "[ReverbEffect]")
{
    const float reverbTimes[ipl::Bands::kNumBands] = { 2.5f, 1.5f, 0.8f };
    const float equalTimes[ipl::Bands::kNumBands] = { 1.5f, 1.5f, 1.5f };

    const struct { double frequency; double expected; } cases[] = {
        { 100.0, 2.5 }, { 2500.0, 1.5 }, { 14000.0, 0.8 },
    };

    for (const auto seed : { 1u, 2u })
    {
        const auto response = impulseResponse(reverbTimes, 8.0, seed);
        const auto reference = impulseResponse(equalTimes, 8.0, seed);

        for (const auto& c : cases)
        {
            const auto ratio = reverbTimeAround(response, c.frequency) / reverbTimeAround(reference, c.frequency);
            INFO("draw " << seed << ", around " << c.frequency << " Hz: " << std::fixed << std::setprecision(3) << ratio << " times the time for equal reverb times, expected " << c.expected / 1.5);
            CHECK(std::fabs(ratio / (c.expected / 1.5) - 1.0) < 0.08);
        }
    }
}
