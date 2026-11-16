#include <catch.hpp>

#include <ambisonics_encode_effect.h>
#include <ambisonics_panning_effect.h>
#include <audio_buffer.h>
#include <panning_effect.h>

namespace
{
const int kFrameSize = 64;
}

TEST_CASE("A unit-amplitude source encoded by Steam Audio is a unit-amplitude AmbiX (SN3D) signal after conversion.", "[AudioBuffer]")
{
    ipl::AmbisonicsEncodeEffectSettings settings{};
    settings.maxOrder = 1;
    ipl::AmbisonicsEncodeEffect effect(settings);

    ipl::AudioBuffer in(1, kFrameSize);
    ipl::AudioBuffer n3d(4, kFrameSize);
    ipl::AudioBuffer sn3d(4, kFrameSize);
    for (auto j = 0; j < kFrameSize; ++j)
    {
        in[0][j] = 1.0f;
    }

    // Straight ahead. AmbiX (ACN, SN3D) for a plane wave of unit amplitude from the front: W = 1, Y = 0, Z = 0, X = 1.
    auto front = ipl::Vector3f(0.0f, 0.0f, -1.0f);
    ipl::AmbisonicsEncodeEffectParams params{};
    params.direction = &front;
    params.order = 1;

    effect.apply(params, in, n3d);
    effect.apply(params, in, n3d);

    ipl::AudioBuffer::convertAmbisonics(ipl::AmbisonicsType::N3D, ipl::AmbisonicsType::SN3D, n3d, sn3d);

    auto last = kFrameSize - 1;
    INFO("SN3D after conversion: W " << sn3d[0][last] << ", Y " << sn3d[1][last] << ", Z " << sn3d[2][last] << ", X " << sn3d[3][last] << " (AmbiX: 1, 0, 0, 1)");

    REQUIRE(fabsf(sn3d[0][last] - 1.0f) < 0.01f);
    REQUIRE(fabsf(sn3d[1][last]) < 0.01f);
    REQUIRE(fabsf(sn3d[2][last]) < 0.01f);
    REQUIRE(fabsf(sn3d[3][last] - 1.0f) < 0.01f);
}

TEST_CASE("A unit-amplitude AmbiX (SN3D) signal decodes to the same level as the same source panned directly.", "[AudioBuffer]")
{
    ipl::AudioSettings audioSettings{};
    audioSettings.samplingRate = 48000;
    audioSettings.frameSize = kFrameSize;

    ipl::SpeakerLayout layout(ipl::SpeakerLayoutType::Stereo);
    ipl::AmbisonicsPanningEffectSettings settings{};
    settings.speakerLayout = &layout;
    settings.maxOrder = 1;
    ipl::AmbisonicsPanningEffect effect(audioSettings, settings);

    // AmbiX for a unit-amplitude plane wave from the front.
    ipl::AudioBuffer sn3d(4, kFrameSize);
    ipl::AudioBuffer n3d(4, kFrameSize);
    ipl::AudioBuffer out(2, kFrameSize);
    const float ambix[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    for (auto c = 0; c < 4; ++c)
    {
        for (auto j = 0; j < kFrameSize; ++j)
        {
            sn3d[c][j] = ambix[c];
        }
    }

    ipl::AudioBuffer::convertAmbisonics(ipl::AmbisonicsType::SN3D, ipl::AmbisonicsType::N3D, sn3d, n3d);

    ipl::AmbisonicsPanningEffectParams params{};
    params.order = 1;
    effect.apply(params, n3d, out);

    // Direct panning of a unit-amplitude source from the front puts 1/sqrt(2) in each channel.
    auto front = ipl::Vector3f(0.0f, 0.0f, -1.0f);
    auto expected = ipl::PanningEffect::panningWeight(front, layout, 0);
    INFO("decoded level " << out[0][0] << ", direct panning " << expected);

    REQUIRE(fabsf(out[0][0] - expected) < 0.25f * expected);
}
