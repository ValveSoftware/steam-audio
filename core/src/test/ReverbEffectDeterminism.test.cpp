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

#include <cstdlib>
#include <vector>

#include <audio_buffer.h>
#include <reverb_effect.h>

namespace {

std::vector<float> impulseResponse(int samplingRate,
                                   int frameSize,
                                   int numFrames)
{
    ipl::ReverbEffect effect(ipl::AudioSettings{ samplingRate, frameSize });

    ipl::Reverb reverb;
    for (auto band = 0; band < ipl::Bands::kNumBands; ++band)
    {
        reverb.reverbTimes[band] = 1.5f;
    }

    ipl::ReverbEffectParams params{};
    params.reverb = &reverb;

    ipl::AudioBuffer in(1, frameSize);
    ipl::AudioBuffer out(1, frameSize);

    std::vector<float> response;
    for (auto frame = 0; frame < numFrames; ++frame)
    {
        in.makeSilent();
        if (frame == 0)
        {
            in[0][0] = 1.0f;
        }

        effect.apply(params, in, out);
        response.insert(response.end(), out[0], out[0] + frameSize);
    }

    return response;
}

}

// Two parametric reverbs with the same settings have to sound the same: the delay lines were drawn with rand(), so
// each reverb had its own, and it also took numbers from the random sequence of the application.
TEST_CASE("The parametric reverb is the same every time", "[ReverbEffect]")
{
    for (const auto samplingRate : { 44100, 48000 })
    {
        const auto first = impulseResponse(samplingRate, 512, 400);

        auto numDifferent = 0;
        for (auto i = 0; i < 20; ++i)
        {
            if (impulseResponse(samplingRate, 512, 400) != first)
            {
                ++numDifferent;
            }
        }

        INFO("sampling rate " << samplingRate << ": " << numDifferent << " of 20 other reverbs differ from the first");
        CHECK(numDifferent == 0);
    }
}

TEST_CASE("The parametric reverb does not use the random sequence of the application", "[ReverbEffect]")
{
    srand(1234);
    const auto expected = rand();

    srand(1234);
    ipl::ReverbEffect effect(ipl::AudioSettings{ 48000, 512 });

    CHECK(rand() == expected);
}
