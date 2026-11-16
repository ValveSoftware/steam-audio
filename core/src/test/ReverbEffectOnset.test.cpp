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

#include <audio_buffer.h>
#include <reverb_effect.h>

// The hybrid reverb sets the gain of its tail from calcOnsetPower(), so the response of the reverb has to follow
// calcOnsetPower() times the decay of the reverb time, whatever the reverb time.
TEST_CASE("The response of the reverb starts at its onset power", "[ReverbEffect]")
{
    const auto samplingRate = 48000;
    const auto frameSize = 256;

    for (auto reverbTime : { 0.5f, 1.0f, 2.0f })
    {
        ipl::ReverbEffect effect(ipl::AudioSettings{ samplingRate, frameSize });

        ipl::Reverb reverb;
        for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
        {
            reverb.reverbTimes[band] = reverbTime;
        }

        float onsetPower[ipl::Bands::kNumBands];
        effect.calcOnsetPower(reverb.reverbTimes, onsetPower);

        ipl::ReverbEffectParams params{};
        params.reverb = &reverb;

        ipl::AudioBuffer in(1, frameSize);
        ipl::AudioBuffer out(1, frameSize);

        const auto numFrames = static_cast<int>(0.3f * samplingRate) / frameSize;
        std::vector<float> response(numFrames * frameSize);
        for (auto frame = 0; frame < numFrames; ++frame)
        {
            in.makeSilent();
            if (frame == 0)
            {
                in[0][0] = 1.0f;
            }

            effect.apply(params, in, out);
            std::copy(out[0], out[0] + frameSize, &response[frame * frameSize]);
        }

        // Between 0.1 s (the response is sparse before) and 0.3 s.
        auto measured = 0.0;
        auto expected = 0.0;
        for (auto i = static_cast<int>(0.1f * samplingRate); i < static_cast<int>(response.size()); ++i)
        {
            const auto time = static_cast<double>(i) / samplingRate;
            measured += static_cast<double>(response[i]) * response[i];
            expected += onsetPower[0] * pow(10.0, -6.0 * time / reverbTime);
        }

        INFO("reverb time " << reverbTime << " s");
        CHECK(std::fabs(10.0 * log10(measured / expected)) < 1.5);
    }
}
