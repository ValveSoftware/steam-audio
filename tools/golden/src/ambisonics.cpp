// Golden data for stage 3.4: ambisonics_encode_effect, ambisonics_rotate_effect, ambisonics_panning_effect,
// ambisonics_binaural_effect and ambisonics_decode_effect.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/ambisonics.rs
// replays the same order. Each case runs a script of operations; the channel count of an output depends on the
// operation's order, so every case writes one flat array of all outputs (in operation order) and a row
// [state, numTailSamplesRemaining] per output with state 0 = TailRemaining, 1 = TailComplete. Output buffers are
// filled with kSentinel before every operation, so channels an effect leaves untouched are recorded too.

#include <ambisonics_binaural_effect.h>
#include <ambisonics_decode_effect.h>
#include <ambisonics_encode_effect.h>
#include <ambisonics_panning_effect.h>
#include <ambisonics_rotate_effect.h>
#include <hrtf_database.h>
#include <sh.h>
#include <speaker_layout.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

constexpr int kSamplingRate = 48000;
constexpr float kSentinel = 9.0f;

Vector3f drawVector(Lcg& lcg)
{
    const float x = lcg.nextSigned();
    const float y = lcg.nextSigned();
    const float z = lcg.nextSigned();
    return Vector3f(x, y, z);
}

// An orientation with a random ahead vector (CoordinateSpace3f's single-vector constructor completes the basis).
CoordinateSpace3f drawOrientation(Lcg& lcg)
{
    const auto ahead = Vector3f::unitVector(drawVector(lcg));
    return CoordinateSpace3f(ahead, Vector3f::kZero);
}

void fill(Lcg& lcg, AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
    {
        for (int j = 0; j < buffer.numSamples(); ++j)
            buffer[i][j] = lcg.nextSigned();
    }
}

void fillSentinel(AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
    {
        for (int j = 0; j < buffer.numSamples(); ++j)
            buffer[i][j] = kSentinel;
    }
}

int numCoeffs(char digit)
{
    return SphericalHarmonics::numCoeffsForOrder(digit - '0');
}

struct Recorder
{
    std::vector<float> outputs;
    std::vector<int32_t> states;

    void record(const AudioBuffer& out, AudioEffectState state, int tailSamplesRemaining)
    {
        for (int i = 0; i < out.numChannels(); ++i)
            outputs.insert(outputs.end(), out[i], out[i] + out.numSamples());
        states.push_back(state == AudioEffectState::TailRemaining ? 0 : 1);
        states.push_back(tailSamplesRemaining);
    }

    void write(const std::filesystem::path& out, const std::string& name) const
    {
        writeNpy(out / (name + ".npy"), {outputs.size()}, outputs.data());
        writeNpy(out / (name + "_states.npy"), {states.size() / 2, 2}, states.data());
    }
};

// A script is a sequence of operations. An operation is a letter, optionally followed by an order digit.
struct ScriptCase
{
    const char* name;
    int maxOrder;
    int frameSize;
    const char* script;
};

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsEncodeEffect
// --------------------------------------------------------------------------------------------------------------------

// A<order>: apply with a random direction; Z<order>: apply with the zero direction; T: tail (order 3 output);
// R: reset.
constexpr ScriptCase kEncodeCases[] = {
    {"max3", 3, 37, "A3A3A3A1A2A0Z3A3RA3TA3"},
    {"max1", 1, 23, "A3A1A3Z1A0A3"},
};

void generateEncode(const std::filesystem::path& out)
{
    Lcg lcg(67);

    for (const auto& c : kEncodeCases)
    {
        AmbisonicsEncodeEffectSettings settings{};
        settings.maxOrder = c.maxOrder;
        AmbisonicsEncodeEffect effect(settings);

        AudioBuffer in(1, c.frameSize);
        Recorder recorder;

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'A':
            case 'Z':
            {
                const bool zero = (*op == 'Z');
                ++op;
                Vector3f direction = zero ? Vector3f(0.0f, 0.0f, 0.0f) : drawVector(lcg);
                fill(lcg, in);
                AudioBuffer output(numCoeffs(*op), c.frameSize);
                fillSentinel(output);
                AmbisonicsEncodeEffectParams params{};
                params.direction = &direction;
                params.order = *op - '0';
                const auto state = effect.apply(params, in, output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'T':
            {
                AudioBuffer output(16, c.frameSize);
                fillSentinel(output);
                const auto state = effect.tail(output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'R':
                effect.reset();
                break;
            }
        }

        recorder.write(out, c.name);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsRotateEffect
// --------------------------------------------------------------------------------------------------------------------

// A<order>: apply with a random orientation; I<order>: apply with the identity orientation; T: tail (order 3
// output); R: reset.
constexpr ScriptCase kRotateCases[] = {
    {"max3", 3, 29, "A3A3A3I3A2A1A0A3RA3TA3"},
    {"max2", 2, 19, "A3A2A3I3A1"},
};

void generateRotate(const std::filesystem::path& out)
{
    Lcg lcg(71);

    for (const auto& c : kRotateCases)
    {
        AudioSettings audioSettings{};
        audioSettings.samplingRate = kSamplingRate;
        audioSettings.frameSize = c.frameSize;

        AmbisonicsRotateEffectSettings settings{};
        settings.maxOrder = c.maxOrder;
        AmbisonicsRotateEffect effect(audioSettings, settings);

        Recorder recorder;

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'A':
            case 'I':
            {
                const bool identity = (*op == 'I');
                ++op;
                auto orientation = identity ? CoordinateSpace3f{} : drawOrientation(lcg);
                AudioBuffer in(numCoeffs(*op), c.frameSize);
                fill(lcg, in);
                AudioBuffer output(numCoeffs(*op), c.frameSize);
                fillSentinel(output);
                AmbisonicsRotateEffectParams params{};
                params.orientation = &orientation;
                params.order = *op - '0';
                const auto state = effect.apply(params, in, output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'T':
            {
                AudioBuffer output(16, c.frameSize);
                fillSentinel(output);
                const auto state = effect.tail(output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'R':
                effect.reset();
                break;
            }
        }

        recorder.write(out, c.name);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsPanningEffect
// --------------------------------------------------------------------------------------------------------------------

// A custom layout with an LFE channel (position (0, 0, 0)), as in spatialization.cpp.
const Vector3f kCustomSpeakers[] = {
    Vector3f(-1.0f, 0.0f, -1.0f),
    Vector3f(1.0f, 0.0f, -1.0f),
    Vector3f(0.0f, 0.0f, 0.0f),
    Vector3f(0.0f, 1.0f, 0.0f),
    Vector3f(0.3f, -0.5f, 0.8f),
};

struct LayoutCase
{
    const char* name;
    SpeakerLayoutType type;
};

constexpr LayoutCase kLayouts[] = {
    {"mono", SpeakerLayoutType::Mono},
    {"stereo", SpeakerLayoutType::Stereo},
    {"quad", SpeakerLayoutType::Quadraphonic},
    {"surround51", SpeakerLayoutType::FivePointOne},
    {"surround71", SpeakerLayoutType::SevenPointOne},
    {"custom", SpeakerLayoutType::Custom},
};

SpeakerLayout makeLayout(SpeakerLayoutType type)
{
    if (type == SpeakerLayoutType::Custom)
        return SpeakerLayout(5, kCustomSpeakers);
    return SpeakerLayout(type);
}

// A<order>: apply; T: tail; R: reset. Lower orders after higher ones exercise the stale rows of the effect's
// Ambisonics matrix (risk R15).
void generatePanning(const std::filesystem::path& out)
{
    Lcg lcg(73);
    constexpr int kMaxOrders[] = {3, 1};
    constexpr int kFrameSize = 31;

    for (const auto maxOrder : kMaxOrders)
    {
        const char* script = (maxOrder == 3) ? "A3A3A2A1A0TA3RA1" : "A1A0A1";

        for (const auto& c : kLayouts)
        {
            const auto layout = makeLayout(c.type);

            AudioSettings audioSettings{};
            audioSettings.samplingRate = kSamplingRate;
            audioSettings.frameSize = kFrameSize;

            AmbisonicsPanningEffectSettings settings{};
            settings.speakerLayout = &layout;
            settings.maxOrder = maxOrder;
            AmbisonicsPanningEffect effect(audioSettings, settings);

            AudioBuffer output(layout.numSpeakers, kFrameSize);
            Recorder recorder;

            for (const char* op = script; *op; ++op)
            {
                switch (*op)
                {
                case 'A':
                {
                    ++op;
                    AudioBuffer in(numCoeffs(*op), kFrameSize);
                    fill(lcg, in);
                    fillSentinel(output);
                    AmbisonicsPanningEffectParams params{};
                    params.order = *op - '0';
                    const auto state = effect.apply(params, in, output);
                    recorder.record(output, state, effect.numTailSamplesRemaining());
                    break;
                }
                case 'T':
                {
                    fillSentinel(output);
                    const auto state = effect.tail(output);
                    recorder.record(output, state, effect.numTailSamplesRemaining());
                    break;
                }
                case 'R':
                    effect.reset();
                    break;
                }
            }

            recorder.write(out, std::string(c.name) + "_max" + std::to_string(maxOrder));
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsBinauralEffect
// --------------------------------------------------------------------------------------------------------------------

HRTFDatabase makeDatabase(int samplingRate, int frameSize)
{
    HRTFSettings settings{};
    settings.type = HRTFMapType::Default;
    settings.hrtfData = nullptr;
    settings.volume = 0.0f;
    settings.normType = HRTFNormType::None;
    return HRTFDatabase(settings, samplingRate, frameSize);
}

// A<order>: apply; T: tail; R: reset; S: switch to the other database (44100 Hz, a different HRIR length).
constexpr ScriptCase kBinauralCases[] = {
    {"max3", 3, 64, "A3A3A3TTA2A1TTTRA3A0"},
    {"max1", 1, 100, "A1A1A0TTTA1"},
    {"max2", 2, 77, "A2A1TA2"},
    {"switch_database", 3, 64, "A3A3SA3A2TTSA3A3"},
};

void generateBinaural(const std::filesystem::path& out)
{
    Lcg lcg(79);

    for (const auto& c : kBinauralCases)
    {
        auto database = makeDatabase(kSamplingRate, c.frameSize);
        auto other = makeDatabase(44100, c.frameSize);
        HRTFDatabase* current = &database;

        AudioSettings audioSettings{};
        audioSettings.samplingRate = kSamplingRate;
        audioSettings.frameSize = c.frameSize;

        AmbisonicsBinauralEffectSettings settings{};
        settings.maxOrder = c.maxOrder;
        settings.hrtf = &database;
        AmbisonicsBinauralEffect effect(audioSettings, settings);

        AudioBuffer output(2, c.frameSize);
        Recorder recorder;

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'A':
            {
                ++op;
                AudioBuffer in(numCoeffs(*op), c.frameSize);
                fill(lcg, in);
                fillSentinel(output);
                AmbisonicsBinauralEffectParams params{};
                params.hrtf = current;
                params.order = *op - '0';
                const auto state = effect.apply(params, in, output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'T':
            {
                fillSentinel(output);
                const auto state = effect.tail(output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'R':
                effect.reset();
                break;
            case 'S':
                current = (current == &database) ? &other : &database;
                break;
            }
        }

        recorder.write(out, c.name);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsDecodeEffect
// --------------------------------------------------------------------------------------------------------------------

// P<order>: apply with panning; B<order>: apply with binaural requested; N<order>: binaural requested without an
// HRTF in the params; T: tail; R: reset. Every apply draws an orientation and an input.
struct DecodeCase
{
    const char* name;
    SpeakerLayoutType layout;
    bool withHrtf;
    int maxOrder;
    int frameSize;
    const char* script;
};

constexpr DecodeCase kDecodeCases[] = {
    {"stereo", SpeakerLayoutType::Stereo, true, 3, 64, "P3P3B3B3TP3P1B1B3TTP2N3RB3P3"},
    {"stereo_no_hrtf", SpeakerLayoutType::Stereo, false, 2, 48, "B2P2B1TP2"},
    {"surround51", SpeakerLayoutType::FivePointOne, true, 3, 64, "P3B3P1P3TRB2"},
    {"stereo_max1", SpeakerLayoutType::Stereo, true, 1, 100, "B1B1P0P1TB1TT"},
};

void generateDecode(const std::filesystem::path& out)
{
    Lcg lcg(83);

    for (const auto& c : kDecodeCases)
    {
        const auto layout = makeLayout(c.layout);
        auto database = makeDatabase(kSamplingRate, c.frameSize);

        AudioSettings audioSettings{};
        audioSettings.samplingRate = kSamplingRate;
        audioSettings.frameSize = c.frameSize;

        AmbisonicsDecodeEffectSettings settings{};
        settings.speakerLayout = &layout;
        settings.maxOrder = c.maxOrder;
        settings.hrtf = c.withHrtf ? &database : nullptr;
        AmbisonicsDecodeEffect effect(audioSettings, settings);

        AudioBuffer output(layout.numSpeakers, c.frameSize);
        Recorder recorder;

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'P':
            case 'B':
            case 'N':
            {
                const char kind = *op;
                ++op;
                auto orientation = drawOrientation(lcg);
                AudioBuffer in(numCoeffs(*op), c.frameSize);
                fill(lcg, in);
                fillSentinel(output);
                AmbisonicsDecodeEffectParams params{};
                params.order = *op - '0';
                params.orientation = &orientation;
                params.binaural = (kind != 'P');
                params.hrtf = (kind == 'N') ? nullptr : &database;
                const auto state = effect.apply(params, in, output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'T':
            {
                fillSentinel(output);
                const auto state = effect.tail(output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'R':
                effect.reset();
                break;
            }
        }

        recorder.write(out, c.name);
    }
}

Register encode("ambisonics_encode_effect", generateEncode);
Register rotate("ambisonics_rotate_effect", generateRotate);
Register panning("ambisonics_panning_effect", generatePanning);
Register binaural("ambisonics_binaural_effect", generateBinaural);
Register decode("ambisonics_decode_effect", generateDecode);

}
