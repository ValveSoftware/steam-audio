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

#include <vector>

#include <delay.h>

namespace {

// The response of an allpass to a unit impulse.
std::vector<float> impulseResponse(ipl::Allpass& allpass,
                                   int numSamples)
{
    std::vector<float> response(numSamples);
    for (auto i = 0; i < numSamples; ++i)
    {
        response[i] = allpass.apply((i == 0) ? 1.0f : 0.0f);
    }

    return response;
}

}

// The allpass filters that diffuse the output of the parametric reverb have to be allpass filters. The coefficients
// that resize() sets must survive reset(), which only clears the state.
TEST_CASE("An allpass keeps its gain when it is reset", "[Allpass]")
{
    const auto delay = 225;
    const auto gain = 0.5f;

    ipl::Allpass allpass(delay, gain, 0);

    // For a gain g and a delay D, H(z) = (z^-D - g) / (1 - g z^-D): h[0] = -g, h[D] = 1 - g^2, h[2D] = g * (1 - g^2).
    auto response = impulseResponse(allpass, 3 * delay);
    CHECK(response[0] == Approx(-gain));
    CHECK(response[delay] == Approx(1.0f - gain * gain));
    CHECK(response[2 * delay] == Approx(gain * (1.0f - gain * gain)));

    allpass.reset();

    response = impulseResponse(allpass, 3 * delay);
    CHECK(response[0] == Approx(-gain));
    CHECK(response[delay] == Approx(1.0f - gain * gain));
    CHECK(response[2 * delay] == Approx(gain * (1.0f - gain * gain)));
}

// The diffusers of the parametric reverb are made like this.
TEST_CASE("An allpass that is resized and reset is not a pure delay", "[Allpass]")
{
    ipl::Allpass allpass;
    allpass.resize(225, 0.5f, 0);
    allpass.reset();

    const auto response = impulseResponse(allpass, 700);
    CHECK(response[0] == Approx(-0.5f));
    CHECK(response[225] == Approx(0.75f));
}
