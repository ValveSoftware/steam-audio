#include <catch.hpp>

#include <ambisonics_encode_effect.h>
#include <audio_buffer.h>
#include <panning_effect.h>
#include <sh.h>

namespace
{
const int kFrameSize = 256;
}

TEST_CASE("Ambisonics encode effect crossfades from the previous direction to the current one over the frame.", "[AmbisonicsEncodeEffect]")
{
    ipl::AmbisonicsEncodeEffectSettings settings{};
    settings.maxOrder = 1;
    ipl::AmbisonicsEncodeEffect effect(settings);

    ipl::AudioBuffer in(1, kFrameSize);
    ipl::AudioBuffer out(4, kFrameSize);
    for (auto j = 0; j < kFrameSize; ++j)
    {
        in[0][j] = 1.0f;
    }

    // Straight ahead, then straight behind. Channel 3 is the front-back dipole, which flips sign between the two.
    auto front = ipl::Vector3f(0.0f, 0.0f, -1.0f);
    auto back = ipl::Vector3f(0.0f, 0.0f, 1.0f);

    ipl::AmbisonicsEncodeEffectParams params{};
    params.order = 1;

    params.direction = &front;
    effect.apply(params, in, out);

    params.direction = &back;
    effect.apply(params, in, out);

    auto weightFront = ipl::SphericalHarmonics::evaluate(1, 1, front);
    auto weightBack = ipl::SphericalHarmonics::evaluate(1, 1, back);

    // The frame starts at the previous direction and ends (almost) at the current one, with a ramp in between.
    REQUIRE(fabsf(out[3][0] - weightFront) < 0.01f);
    REQUIRE(fabsf(out[3][kFrameSize - 1] - weightBack) < 0.02f);
    REQUIRE(fabsf(out[3][kFrameSize / 2]) < 0.02f);
}

TEST_CASE("Panning effect crossfades from the previous direction to the current one over the frame.", "[PanningEffect]")
{
    ipl::SpeakerLayout layout(ipl::SpeakerLayoutType::Stereo);

    ipl::PanningEffectSettings settings{};
    settings.speakerLayout = &layout;
    ipl::PanningEffect effect(settings);

    ipl::AudioBuffer in(1, kFrameSize);
    ipl::AudioBuffer out(2, kFrameSize);
    for (auto j = 0; j < kFrameSize; ++j)
    {
        in[0][j] = 1.0f;
    }

    auto left = ipl::Vector3f(-1.0f, 0.0f, 0.0f);
    auto right = ipl::Vector3f(1.0f, 0.0f, 0.0f);

    ipl::PanningEffectParams params{};

    params.direction = &left;
    effect.apply(params, in, out);

    params.direction = &right;
    effect.apply(params, in, out);

    // Hard left to hard right: the left channel goes from 1 to 0 and the right channel from 0 to 1.
    REQUIRE(fabsf(out[0][0] - 1.0f) < 0.02f);
    REQUIRE(fabsf(out[0][kFrameSize - 1]) < 0.02f);
    REQUIRE(fabsf(out[1][0]) < 0.02f);
    REQUIRE(fabsf(out[1][kFrameSize - 1] - 1.0f) < 0.02f);
}
