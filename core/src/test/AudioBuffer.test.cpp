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

#include <audio_buffer.h>
#include <sh.h>
using namespace ipl;


TEST_CASE("Mixing audio buffers works.", "[AudioBuffer]")
{
    AudioBuffer in1(1, 2);
    AudioBuffer in2(1, 2);
    AudioBuffer in3(1, 2);

    in1[0][0] = 1.0f;
    in1[0][1] = 2.0f;
    in2[0][0] = 3.0f;
    in2[0][1] = 4.0f;
    in3[0][0] = 7.0f;
    in3[0][1] = 9.0f;

    AudioBuffer out(1, 2);

    out.makeSilent();

    AudioBuffer::mix(in1, out);
    AudioBuffer::mix(in2, out);
    AudioBuffer::mix(in3, out);

    REQUIRE(out[0][0] == Approx(11.0));
    REQUIRE(out[0][1] == Approx(15.0));
}

TEST_CASE("Deinterleaving an interleaved buffer works.", "[AudioBuffer]")
{
    float interleaved[4] = { 1.0f, 2.0f, 1.0f, 2.0f };

    AudioBuffer deinterleaved(2, 2);

    deinterleaved.write(interleaved);

    REQUIRE(deinterleaved[0][0] == Approx(1.0));
    REQUIRE(deinterleaved[0][1] == Approx(1.0));
    REQUIRE(deinterleaved[1][0] == Approx(2.0));
    REQUIRE(deinterleaved[1][1] == Approx(2.0));
}

TEST_CASE("Interleaving a deinterleaved buffer works.", "[AudioBuffer]")
{
    AudioBuffer deinterleaved(2, 2);
    deinterleaved[0][0] = 1.0f;
    deinterleaved[0][1] = 1.0f;
    deinterleaved[1][0] = 2.0f;
    deinterleaved[1][1] = 2.0f;

    float interleaved[4];

    deinterleaved.read(interleaved);

    REQUIRE(interleaved[0] == Approx(1.0));
    REQUIRE(interleaved[1] == Approx(2.0));
    REQUIRE(interleaved[2] == Approx(1.0));
    REQUIRE(interleaved[3] == Approx(2.0));
}

TEST_CASE("Downmixing to mono works.", "[AudioBuffer]")
{
    AudioBuffer stereo(2, 2);
    stereo[0][0] = 1.0f;
    stereo[0][1] = 1.0f;
    stereo[1][0] = 2.0f;
    stereo[1][1] = 2.0f;

    AudioBuffer mono(1, 2);

    AudioBuffer::downmix(stereo, mono);

    REQUIRE(mono[0][0] == Approx(1.5));
    REQUIRE(mono[0][1] == Approx(1.5));
}

TEST_CASE("Ambisonics to Ambisonics format conversion works.", "[AudioBuffer]")
{
    auto order = 2;
    auto numChannels = SphericalHarmonics::numCoeffsForOrder(order);

    AudioBuffer n3dBuffer(numChannels, 3);
    AudioBuffer sn3dBuffer(numChannels, 3);
    AudioBuffer fumaBuffer(numChannels, 3);
    AudioBuffer testBuffer(numChannels, 3);

    for (auto i = 0; i < n3dBuffer.numChannels(); ++i)
    {
        for (auto j = 0; j < n3dBuffer.numSamples(); ++j)
        {
            n3dBuffer[i][j] = static_cast<float>(j);
        }
    }

    SECTION("N3D -> SN3D -> N3D")
    {
        AudioBuffer::convertAmbisonics(AmbisonicsType::N3D, AmbisonicsType::SN3D, n3dBuffer, sn3dBuffer);
        AudioBuffer::convertAmbisonics(AmbisonicsType::SN3D, AmbisonicsType::N3D, sn3dBuffer, testBuffer);

        for (auto i = 0; i < testBuffer.numChannels(); ++i)
            for (auto j = 0; j < testBuffer.numSamples(); ++j)
                REQUIRE(testBuffer[i][j] == Approx(n3dBuffer[i][j]));
    }

    SECTION("N3D -> FuMa -> N3D")
    {
        AudioBuffer::convertAmbisonics(AmbisonicsType::N3D, AmbisonicsType::FuMa, n3dBuffer, fumaBuffer);
        AudioBuffer::convertAmbisonics(AmbisonicsType::FuMa, AmbisonicsType::N3D, fumaBuffer, testBuffer);

        for (auto i = 0; i < testBuffer.numChannels(); ++i)
            for (auto j = 0; j < testBuffer.numSamples(); ++j)
                REQUIRE(testBuffer[i][j] == Approx(n3dBuffer[i][j]));
    }

    SECTION("N3D -> SN3D -> FuMa -> SN3D -> N3D")
    {
        AudioBuffer::convertAmbisonics(AmbisonicsType::N3D, AmbisonicsType::SN3D, n3dBuffer, sn3dBuffer);
        AudioBuffer::convertAmbisonics(AmbisonicsType::SN3D, AmbisonicsType::FuMa, sn3dBuffer, fumaBuffer);
        AudioBuffer::convertAmbisonics(AmbisonicsType::FuMa, AmbisonicsType::SN3D, fumaBuffer, sn3dBuffer);
        AudioBuffer::convertAmbisonics(AmbisonicsType::SN3D, AmbisonicsType::N3D, sn3dBuffer, testBuffer);

        for (auto i = 0; i < testBuffer.numChannels(); ++i)
            for (auto j = 0; j < testBuffer.numSamples(); ++j)
                REQUIRE(testBuffer[i][j] == Approx(n3dBuffer[i][j]));
    }

    SECTION("N3D -> SN3D -> FuMa -> N3D")
    {
        AudioBuffer::convertAmbisonics(AmbisonicsType::N3D, AmbisonicsType::SN3D, n3dBuffer, sn3dBuffer);
        AudioBuffer::convertAmbisonics(AmbisonicsType::SN3D, AmbisonicsType::FuMa, sn3dBuffer, fumaBuffer);
        AudioBuffer::convertAmbisonics(AmbisonicsType::FuMa, AmbisonicsType::N3D, fumaBuffer, testBuffer);

        for (auto i = 0; i < testBuffer.numChannels(); ++i)
            for (auto j = 0; j < testBuffer.numSamples(); ++j)
                REQUIRE(testBuffer[i][j] == Approx(n3dBuffer[i][j]));
    }
}