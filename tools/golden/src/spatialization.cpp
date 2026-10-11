// Golden data for stage 3.3: panning_effect.{h,cpp}, binaural_effect.{h,cpp}, virtual_surround_effect.{h,cpp},
// speaker_layout.{h,cpp}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/spatialization.rs
// replays the same order. Each effect case runs a script of one-letter operations; every operation that produces
// audio records its output (channels x frame size) and a row [state, numTailSamplesRemaining] with
// state 0 = TailRemaining, 1 = TailComplete.

#include <binaural_effect.h>
#include <hrtf_database.h>
#include <panning_effect.h>
#include <speaker_layout.h>
#include <virtual_surround_effect.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

constexpr int kSamplingRate = 48000;

Vector3f drawVector(Lcg& lcg)
{
    const float x = lcg.nextSigned();
    const float y = lcg.nextSigned();
    const float z = lcg.nextSigned();
    return Vector3f(x, y, z);
}

void fill(Lcg& lcg, AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
    {
        for (int j = 0; j < buffer.numSamples(); ++j)
            buffer[i][j] = lcg.nextSigned();
    }
}

struct Recorder
{
    std::vector<float> outputs;
    std::vector<int32_t> states;
    size_t count = 0;

    void record(const AudioBuffer& out, AudioEffectState state, int tailSamplesRemaining)
    {
        for (int i = 0; i < out.numChannels(); ++i)
            outputs.insert(outputs.end(), out[i], out[i] + out.numSamples());
        states.push_back(state == AudioEffectState::TailRemaining ? 0 : 1);
        states.push_back(tailSamplesRemaining);
        ++count;
    }

    void write(const std::filesystem::path& out, const std::string& name, int numChannels, int frameSize) const
    {
        writeNpy(out / (name + ".npy"), {count, static_cast<size_t>(numChannels), static_cast<size_t>(frameSize)},
                 outputs.data());
        writeNpy(out / (name + "_states.npy"), {count, 2}, states.data());
    }
};

// A custom layout with an LFE channel (position (0, 0, 0)); it must outlive the effects that copy the layout.
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

// --------------------------------------------------------------------------------------------------------------------
// PanningEffect
// --------------------------------------------------------------------------------------------------------------------

// Directions evaluated before the random ones: the axes and the zero vector.
const Vector3f kFixedDirections[] = {
    Vector3f(1.0f, 0.0f, 0.0f),  Vector3f(-1.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f), Vector3f(0.0f, -1.0f, 0.0f),
    Vector3f(0.0f, 0.0f, 1.0f),  Vector3f(0.0f, 0.0f, -1.0f), Vector3f(0.0f, 0.0f, 0.0f),
};

constexpr int kNumWeightDirections = 300;

void generatePanningWeights(const std::filesystem::path& out, Lcg& lcg)
{
    std::vector<Vector3f> directions(std::begin(kFixedDirections), std::end(kFixedDirections));
    for (int i = 0; i < kNumWeightDirections; ++i)
        directions.push_back(drawVector(lcg));

    for (const auto& c : kLayouts)
    {
        const auto layout = makeLayout(c.type);
        std::vector<float> weights;
        for (const auto& direction : directions)
        {
            for (int i = 0; i < layout.numSpeakers; ++i)
                weights.push_back(PanningEffect::panningWeight(direction, layout, i));
        }
        writeNpy(out / (std::string("weights_") + c.name + ".npy"),
                 {directions.size(), static_cast<size_t>(layout.numSpeakers)}, weights.data());
    }
}

// A: apply with a new direction and input; T: tail; R: reset.
void generatePanningEffects(const std::filesystem::path& out, Lcg& lcg)
{
    constexpr int kFrameSize = 37;
    const char* script = "AAARAAATA";

    for (const auto& c : kLayouts)
    {
        const auto layout = makeLayout(c.type);

        PanningEffectSettings settings{};
        settings.speakerLayout = &layout;
        PanningEffect effect(settings);

        AudioBuffer in(1, kFrameSize);
        AudioBuffer output(layout.numSpeakers, kFrameSize);
        Recorder recorder;

        for (const char* op = script; *op; ++op)
        {
            switch (*op)
            {
            case 'A':
            {
                auto direction = drawVector(lcg);
                fill(lcg, in);
                PanningEffectParams params{};
                params.direction = &direction;
                const auto state = effect.apply(params, in, output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'T':
            {
                const auto state = effect.tail(output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'R':
                effect.reset();
                break;
            }
        }

        recorder.write(out, std::string("effect_") + c.name, layout.numSpeakers, kFrameSize);
    }
}

void generatePanning(const std::filesystem::path& out)
{
    Lcg lcg(53);
    generatePanningWeights(out, lcg);
    generatePanningEffects(out, lcg);
}

// --------------------------------------------------------------------------------------------------------------------
// BinauralEffect
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

// A: apply with a random direction; T: tail; R: reset; S: switch to the other database (a different HRIR length).
struct BinauralCase
{
    const char* name;
    int inChannels;
    HRTFInterpolation interpolation;
    float spatialBlend;
    HRTFPhaseType phaseType;
    int frameSize;
    const char* script;
};

constexpr BinauralCase kBinauralCases[] = {
    {"nearest_mono", 1, HRTFInterpolation::NearestNeighbor, 1.0f, HRTFPhaseType::None, 128, "AAAATTTTRAA"},
    {"bilinear_mono", 1, HRTFInterpolation::Bilinear, 1.0f, HRTFPhaseType::None, 77, "AAAATTTTAA"},
    {"nearest_stereo", 2, HRTFInterpolation::NearestNeighbor, 1.0f, HRTFPhaseType::None, 100, "AAATTTA"},
    {"nearest_stereo_blend_none", 2, HRTFInterpolation::NearestNeighbor, 0.4f, HRTFPhaseType::None, 100, "AAATTA"},
    {"nearest_stereo_blend_itd", 2, HRTFInterpolation::NearestNeighbor, 0.7f, HRTFPhaseType::SphereITD, 100, "AAATTA"},
    {"nearest_stereo_blend_full", 2, HRTFInterpolation::NearestNeighbor, 0.2f, HRTFPhaseType::Full, 100, "AAATTA"},
    {"nearest_mono_blend", 1, HRTFInterpolation::NearestNeighbor, 0.5f, HRTFPhaseType::Full, 64, "AAATA"},
    {"bilinear_stereo_blend_itd", 2, HRTFInterpolation::Bilinear, 0.6f, HRTFPhaseType::SphereITD, 100, "AAATTA"},
    {"bilinear_stereo_blend_full", 2, HRTFInterpolation::Bilinear, 0.3f, HRTFPhaseType::Full, 64, "AAATTA"},
    {"bilinear_mono_blend", 1, HRTFInterpolation::Bilinear, 0.5f, HRTFPhaseType::SphereITD, 64, "AAATA"},
    {"switch_database", 1, HRTFInterpolation::NearestNeighbor, 1.0f, HRTFPhaseType::None, 64, "AASAATTSAA"},
};

void generateBinaural(const std::filesystem::path& out)
{
    Lcg lcg(59);

    for (const auto& c : kBinauralCases)
    {
        auto database = makeDatabase(kSamplingRate, c.frameSize);
        auto other = makeDatabase(44100, c.frameSize);
        HRTFDatabase* current = &database;

        AudioSettings audioSettings{};
        audioSettings.samplingRate = kSamplingRate;
        audioSettings.frameSize = c.frameSize;

        BinauralEffectSettings settings{};
        settings.hrtf = &database;
        BinauralEffect effect(audioSettings, settings);

        AudioBuffer in(c.inChannels, c.frameSize);
        AudioBuffer output(2, c.frameSize);
        Recorder recorder;
        std::vector<float> peakDelays;

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'A':
            {
                auto direction = Vector3f::unitVector(drawVector(lcg));
                fill(lcg, in);
                float delays[2] = {};
                BinauralEffectParams params{};
                params.direction = &direction;
                params.interpolation = c.interpolation;
                params.spatialBlend = c.spatialBlend;
                params.phaseType = c.phaseType;
                params.hrtf = current;
                params.peakDelays = delays;
                const auto state = effect.apply(params, in, output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                peakDelays.push_back(delays[0]);
                peakDelays.push_back(delays[1]);
                break;
            }
            case 'T':
            {
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

        // Peak delays are recorded for applies only, so they are written separately from the tail rows.
        recorder.write(out, c.name, 2, c.frameSize);
        writeNpy(out / (std::string(c.name) + "_peak_delays.npy"), {peakDelays.size() / 2, 2}, peakDelays.data());
    }
}

// --------------------------------------------------------------------------------------------------------------------
// VirtualSurroundEffect
// --------------------------------------------------------------------------------------------------------------------

// A: apply; T: tail; R: reset.
void generateVirtualSurround(const std::filesystem::path& out)
{
    Lcg lcg(61);
    constexpr int kFrameSize = 64;
    const char* script = "AATTAARAATTTT";

    for (const auto& c : kLayouts)
    {
        const auto layout = makeLayout(c.type);
        auto database = makeDatabase(kSamplingRate, kFrameSize);

        AudioSettings audioSettings{};
        audioSettings.samplingRate = kSamplingRate;
        audioSettings.frameSize = kFrameSize;

        VirtualSurroundEffectSettings settings{};
        settings.speakerLayout = &layout;
        settings.hrtf = &database;
        VirtualSurroundEffect effect(audioSettings, settings);

        AudioBuffer in(layout.numSpeakers, kFrameSize);
        AudioBuffer output(2, kFrameSize);
        Recorder recorder;

        for (const char* op = script; *op; ++op)
        {
            switch (*op)
            {
            case 'A':
            {
                fill(lcg, in);
                VirtualSurroundEffectParams params{};
                params.hrtf = &database;
                const auto state = effect.apply(params, in, output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'T':
            {
                const auto state = effect.tail(output);
                recorder.record(output, state, effect.numTailSamplesRemaining());
                break;
            }
            case 'R':
                effect.reset();
                break;
            }
        }

        recorder.write(out, c.name, 2, kFrameSize);
    }
}

Register panning("panning_effect", generatePanning);
Register binaural("binaural_effect", generateBinaural);
Register virtualSurround("virtual_surround_effect", generateVirtualSurround);

}
