#include <catch.hpp>

#include <phonon.h>

TEST_CASE("Ambisonics binaural and decode effects refuse orders above those the HRTF data supports.", "[AmbisonicsBinauralEffect]")
{
    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    REQUIRE(iplContextCreate(&contextSettings, &context) == IPL_STATUS_SUCCESS);

    IPLAudioSettings audioSettings = { 48000, 512 };

    IPLHRTFSettings hrtfSettings{ IPL_HRTFTYPE_DEFAULT, nullptr, nullptr, 0, 1.0f, IPL_HRTFNORMTYPE_NONE };
    IPLHRTF hrtf = nullptr;
    REQUIRE(iplHRTFCreate(context, &audioSettings, &hrtfSettings, &hrtf) == IPL_STATUS_SUCCESS);

    // The Ambisonics HRTFs have 16 coefficients (order 3), the same limit that the Ambisonics panning effect enforces.
    IPLAmbisonicsBinauralEffectSettings binauralSettings{ hrtf, 4 };
    IPLAmbisonicsBinauralEffect binaural = nullptr;
    auto binauralStatus = iplAmbisonicsBinauralEffectCreate(context, &audioSettings, &binauralSettings, &binaural);
    INFO("binaural effect, max order 4: status " << static_cast<int>(binauralStatus));
    REQUIRE(binauralStatus != IPL_STATUS_SUCCESS);
    iplAmbisonicsBinauralEffectRelease(&binaural);

    IPLAmbisonicsDecodeEffectSettings decodeSettings{};
    decodeSettings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
    decodeSettings.hrtf = hrtf;
    decodeSettings.maxOrder = 4;
    IPLAmbisonicsDecodeEffect decode = nullptr;
    auto decodeStatus = iplAmbisonicsDecodeEffectCreate(context, &audioSettings, &decodeSettings, &decode);
    INFO("decode effect, max order 4: status " << static_cast<int>(decodeStatus));
    REQUIRE(decodeStatus != IPL_STATUS_SUCCESS);
    iplAmbisonicsDecodeEffectRelease(&decode);

    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);
}
