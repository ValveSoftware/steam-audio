// Golden data for stage 2.4: audio_buffer.{h,cpp}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/audio_buffer.rs
// replays the same order.

#include <audio_buffer.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

// Not a multiple of 4, so both the SIMD body and the scalar tail of ArrayMath are exercised.
constexpr int kNumSamples = 37;

// Fills the buffer channel by channel, sample by sample.
void fill(Lcg& lcg, AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
    {
        for (int j = 0; j < buffer.numSamples(); ++j)
        {
            buffer[i][j] = lcg.nextSigned();
        }
    }
}

void writeBuffer(const std::filesystem::path& path, const AudioBuffer& buffer)
{
    std::vector<float> planar;
    planar.reserve(static_cast<size_t>(buffer.numChannels()) * buffer.numSamples());
    for (int i = 0; i < buffer.numChannels(); ++i)
        planar.insert(planar.end(), buffer[i], buffer[i] + buffer.numSamples());

    writeNpy(path, {static_cast<size_t>(buffer.numChannels()), static_cast<size_t>(buffer.numSamples())},
             planar.data());
}

void generateAudioBuffer(const std::filesystem::path& out)
{
    Lcg lcg(11);

    AudioBuffer planar(3, kNumSamples);
    fill(lcg, planar);

    std::vector<float> interleaved(3 * kNumSamples);
    planar.read(interleaved.data());
    writeNpy(out / "read.npy", {interleaved.size()}, interleaved.data());

    for (auto& x : interleaved)
        x = lcg.nextSigned();
    AudioBuffer deinterleaved(3, kNumSamples);
    deinterleaved.write(interleaved.data());
    writeBuffer(out / "write.npy", deinterleaved);

    AudioBuffer scaled(3, kNumSamples);
    fill(lcg, scaled);
    const float volume = lcg.nextSigned() * 2.0f;
    scaled.scale(volume);
    writeBuffer(out / "scale.npy", scaled);

    AudioBuffer mixIn(3, kNumSamples), mixOut(3, kNumSamples);
    fill(lcg, mixIn);
    fill(lcg, mixOut);
    AudioBuffer::mix(mixIn, mixOut);
    writeBuffer(out / "mix.npy", mixOut);

    // 3 and 5 channels: 1/3 and 1/5 are not exact, so the reciprocal-then-multiply order matters.
    for (int numChannels : {1, 3, 5})
    {
        AudioBuffer in(numChannels, kNumSamples), mono(1, kNumSamples);
        fill(lcg, in);
        AudioBuffer::downmix(in, mono);
        writeBuffer(out / ("downmix_" + std::to_string(numChannels) + ".npy"), mono);
    }
}

const char* typeName(AmbisonicsType type)
{
    switch (type)
    {
    case AmbisonicsType::N3D: return "n3d";
    case AmbisonicsType::SN3D: return "sn3d";
    default: return "fuma";
    }
}

void generateAmbisonicsConversion(const std::filesystem::path& out)
{
    constexpr int kAmbisonicsSamples = 13;
    const AmbisonicsType types[] = {AmbisonicsType::N3D, AmbisonicsType::SN3D, AmbisonicsType::FuMa};

    Lcg lcg(13);

    // Orders 0-3. Inputs are drawn once per order and shared by all nine type pairs.
    for (int order = 0; order <= 3; ++order)
    {
        const int numChannels = (order + 1) * (order + 1);
        AudioBuffer in(numChannels, kAmbisonicsSamples);
        fill(lcg, in);

        for (auto inType : types)
        {
            for (auto outType : types)
            {
                AudioBuffer converted(numChannels, kAmbisonicsSamples);
                AudioBuffer::convertAmbisonics(inType, outType, in, converted);
                writeBuffer(out / ("order" + std::to_string(order) + "_" + typeName(inType) + "_to_" +
                                   typeName(outType) + ".npy"),
                            converted);
            }
        }
    }
}

Register audioBuffer("audio_buffer", generateAudioBuffer);
Register ambisonicsConversion("ambisonics_conversion", generateAmbisonicsConversion);

}
