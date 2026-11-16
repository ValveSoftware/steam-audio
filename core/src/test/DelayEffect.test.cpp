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

#include <vector>

#include <catch.hpp>

#include <delay_effect.h>
using namespace ipl;

TEST_CASE("Delay effect tail is the delayed input followed by silence", "[DelayEffect]")
{
    const auto frameSize = 64;
    const auto delay = 200;
    const auto ringSize = 2048;

    AudioSettings audioSettings(48000, frameSize);
    DelayEffect effect(audioSettings, DelayEffectSettings{ ringSize });
    DelayEffect reference(audioSettings, DelayEffectSettings{ ringSize });

    DelayEffectParams params{};
    params.delayInSamples = delay;

    // Run a long non-zero signal through both effects, so that the ring buffer holds old audio.
    AudioBuffer in(1, frameSize);
    AudioBuffer out(1, frameSize);
    AudioBuffer silence(1, frameSize);
    silence.makeSilent();
    auto n = 0;
    for (auto frame = 0; frame < 100; ++frame)
    {
        for (auto i = 0; i < frameSize; ++i)
            in[0][i] = 1.0f + static_cast<float>(++n % 7);

        auto state = effect.apply(params, in, out);
        reference.apply(params, in, out);
        REQUIRE(state == AudioEffectState::TailRemaining);
    }

    // After the input stops, the tail must be what the effect would output for silent input.
    auto numTailFrames = (delay + frameSize - 1) / frameSize + 1;
    for (auto frame = 0; frame < numTailFrames; ++frame)
    {
        AudioBuffer expected(1, frameSize);
        reference.apply(params, silence, expected);

        AudioBuffer tail(1, frameSize);
        effect.tail(tail);

        auto maxError = 0.0f;
        for (auto i = 0; i < frameSize; ++i)
            maxError = std::max(maxError, fabsf(tail[0][i] - expected[0][i]));

        INFO("tail frame " << frame << ": expected[0] = " << expected[0][0] << ", tail[0] = " << tail[0][0]
                           << ", max error " << maxError);
        CHECK(maxError < 1e-6f);
    }
}
