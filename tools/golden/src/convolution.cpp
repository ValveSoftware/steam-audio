// Golden data for stage 3.2: overlap_add_convolution_effect.{h,cpp}, overlap_save_convolution_effect.{h,cpp}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/convolution.rs
// replays the same order. IR spectra are drawn directly (not transformed from IRs), so the effects' inputs are exact
// on both sides and only the effects' own FFTs differ (R1).
//
// Each effect case runs a script of one-letter operations. Every operation that produces audio records its output
// (channels x frame size) and a row [state, numTailSamplesRemaining] with state 0 = TailRemaining, 1 = TailComplete.
//
// Overlap-save effects and the mixer are mono: upstream's generic x86 ArrayMath::multiplyAccumulate(complex) does an
// aligned load of the accumulator even in its unaligned branch, and every odd channel's accumulator row (N / 2 + 1
// bins) is unaligned, so this (SSE, non-IPP) build crashes with more than one channel. Channels are independent, so
// the Rust tests check multi-channel effects against mono ones.

#include <impulse_response.h>
#include <overlap_add_convolution_effect.h>
#include <overlap_save_convolution_effect.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

void fill(Lcg& lcg, AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
    {
        for (int j = 0; j < buffer.numSamples(); ++j)
            buffer[i][j] = lcg.nextSigned();
    }
}

// A random spectrum of a real signal: the DC and Nyquist bins are real. Upstream's FFT::applyForward returns, as the
// Nyquist bin's imaginary part, the one of the last spectrum passed to applyInverse on the same FFT object (R11). Real
// spectra keep that value at zero, as in every upstream pipeline; the port always returns zero.
void fill(Lcg& lcg, complex_t* spectrum, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float re = lcg.nextSigned();
        const float im = lcg.nextSigned();
        spectrum[i] = complex_t(re, im);
    }
    spectrum[0].imag(0.0f);
    spectrum[n - 1].imag(0.0f);
}

void fill(Lcg& lcg, OverlapSaveFIR& fir)
{
    for (int i = 0; i < fir.numChannels(); ++i)
    {
        for (int j = 0; j < fir.numBlocks(); ++j)
            fill(lcg, fir[i][j], fir.numSpectrumSamples());
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

// --------------------------------------------------------------------------------------------------------------------
// OverlapAddConvolutionEffect
// --------------------------------------------------------------------------------------------------------------------

// I: draw new IR spectra; A: apply (multipleInputs = false); M: apply (multipleInputs = true); T: tail; R: reset.
// Each apply draws an input frame of inChannels channels first.
struct OverlapAddCase
{
    const char* name;
    int numChannels;
    int inChannels;
    int irSize;
    int frameSize;
    const char* script;
};

constexpr OverlapAddCase kOverlapAddCases[] = {
    {"overlap_add_mono", 2, 1, 100, 64, "IAAAIAATTTTAA"},
    {"overlap_add_multi", 3, 3, 50, 37, "IMMAIMMTTTTRMM"},
    {"overlap_add_long", 2, 1, 1000, 128, "IAIAATTTTAAA"},
};

void generateOverlapAdd(const std::filesystem::path& out)
{
    Lcg lcg(41);

    for (const auto& c : kOverlapAddCases)
    {
        AudioSettings audioSettings{};
        audioSettings.samplingRate = 48000;
        audioSettings.frameSize = c.frameSize;

        OverlapAddConvolutionEffectSettings settings{};
        settings.numChannels = c.numChannels;
        settings.irSize = c.irSize;

        OverlapAddConvolutionEffect effect(audioSettings, settings);

        Array<complex_t, 2> ir(c.numChannels, effect.irSpectrumSize());
        ir.zero();
        AudioBuffer in(c.inChannels, c.frameSize);
        AudioBuffer output(c.numChannels, c.frameSize);
        Recorder recorder;

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'I':
                for (int i = 0; i < c.numChannels; ++i)
                    fill(lcg, ir[i], effect.irSpectrumSize());
                break;
            case 'A':
            case 'M':
            {
                fill(lcg, in);
                OverlapAddConvolutionEffectParams params{};
                params.fftIR = ir.data();
                params.multipleInputs = (*op == 'M');
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

        recorder.write(out, c.name, c.numChannels, c.frameSize);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// OverlapSavePartitioner
// --------------------------------------------------------------------------------------------------------------------

void writeFIR(const std::filesystem::path& path, const OverlapSaveFIR& fir)
{
    std::vector<float> data;
    for (int i = 0; i < fir.numChannels(); ++i)
    {
        for (int j = 0; j < fir.numBlocks(); ++j)
        {
            const auto* s = fir[i][j];
            for (int k = 0; k < fir.numSpectrumSamples(); ++k)
            {
                data.push_back(s[k].real());
                data.push_back(s[k].imag());
            }
        }
    }
    writeNpy(path,
             {static_cast<size_t>(fir.numChannels()), static_cast<size_t>(fir.numBlocks()),
              static_cast<size_t>(fir.numSpectrumSamples()), 2},
             data.data());
}

// An order-1 IR (4 channels) of 0.00625 s at 48 kHz (300 samples), partitioned into a 3-channel FIR of 5 blocks of 64:
// first 250 samples of 2 channels (the last block is partial, so it keeps stale samples of the previous block), then
// 150 samples of all channels (stale samples from the previous call), then all 300 samples into a 2-block FIR.
void generatePartitioner(const std::filesystem::path& out)
{
    Lcg lcg(43);

    ImpulseResponse ir(0.00625f, 1, 48000);
    for (int i = 0; i < ir.numChannels(); ++i)
    {
        for (int j = 0; j < ir.numSamples(); ++j)
            ir[i][j] = lcg.nextSigned();
    }
    writeNpy(out / "partition_ir.npy", {static_cast<size_t>(ir.numChannels()), static_cast<size_t>(ir.numSamples())},
             ir.data());

    OverlapSavePartitioner partitioner(64);
    OverlapSaveFIR fir(3, 300, 64);

    partitioner.partition(ir, 2, 250, fir);
    writeFIR(out / "partition_a.npy", fir);

    partitioner.partition(ir, 8, 150, fir);
    writeFIR(out / "partition_b.npy", fir);

    OverlapSaveFIR shortFIR(4, 100, 64);
    partitioner.partition(ir, 4, 1000, shortFIR);
    writeFIR(out / "partition_c.npy", shortFIR);
}

// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveConvolutionEffect
// --------------------------------------------------------------------------------------------------------------------

// U: draw a new FIR and apply with fftIRUpdated; A: apply without an update; T: tail; R: reset. Each apply draws a
// mono input frame after any FIR.
struct OverlapSaveCase
{
    const char* name;
    int irSize;
    int frameSize;
    const char* script;
};

constexpr OverlapSaveCase kOverlapSaveCases[] = {
    {"overlap_save_a", 150, 64, "UAAAUATTTARAUA"},
    {"overlap_save_b", 100, 37, "UAUAATTTUA"},
    {"overlap_save_c", 64, 64, "UATAUT"},
};

void generateOverlapSave(const std::filesystem::path& out)
{
    Lcg lcg(47);

    for (const auto& c : kOverlapSaveCases)
    {
        AudioSettings audioSettings{};
        audioSettings.samplingRate = 48000;
        audioSettings.frameSize = c.frameSize;

        OverlapSaveConvolutionEffect effect(audioSettings, OverlapSaveConvolutionEffectSettings{1, c.irSize});

        OverlapSaveFIR fir(1, c.irSize, c.frameSize);
        AudioBuffer in(1, c.frameSize);
        AudioBuffer output(1, c.frameSize);
        Recorder recorder;

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'U':
            case 'A':
            {
                const bool update = (*op == 'U');
                if (update)
                    fill(lcg, fir);
                fill(lcg, in);

                OverlapSaveConvolutionEffectDirectParams params{};
                params.fftIR = &fir;
                params.fftIRUpdated = update;
                params.numChannels = 1;
                params.numSamples = c.irSize;
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

        recorder.write(out, c.name, 1, c.frameSize);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveConvolutionMixer
// --------------------------------------------------------------------------------------------------------------------

// Two mono effects (IR sizes 150 and 100) mixed into one mono mixer, through the TripleBuffer interface. Per frame,
// each effect either updates its FIR and applies (U), applies (A), outputs its tail (T) or is skipped (-); then the
// mixer outputs mixChannels channels (0 leaves the output silent but still resets the mixer). Effect states are recorded as 0/1, or -1 when skipped.
struct MixerFrame
{
    char ops[2];
    int mixChannels;
};

constexpr MixerFrame kMixerFrames[] = {
    {{'U', 'U'}, 1}, {{'A', 'A'}, 1}, {{'A', 'U'}, 1}, {{'U', 'A'}, 0}, {{'A', '-'}, 1},
    {{'T', 'T'}, 1}, {{'T', 'T'}, 1}, {{'-', 'T'}, 1}, {{'-', '-'}, 1},
};

void generateMixer(const std::filesystem::path& out)
{
    constexpr int kFrameSize = 64;
    constexpr int kNumChannels = 1;
    constexpr int kIRSizes[] = {150, 100};

    Lcg lcg(53);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 48000;
    audioSettings.frameSize = kFrameSize;

    OverlapSaveConvolutionMixer mixer(audioSettings, OverlapSaveConvolutionEffectSettings{kNumChannels, 150});

    std::vector<std::unique_ptr<OverlapSaveConvolutionEffect>> effects;
    std::vector<std::unique_ptr<TripleBuffer<OverlapSaveFIR>>> firs;
    for (int irSize : kIRSizes)
    {
        effects.push_back(std::make_unique<OverlapSaveConvolutionEffect>(
            audioSettings, OverlapSaveConvolutionEffectSettings{kNumChannels, irSize}));
        firs.push_back(std::make_unique<TripleBuffer<OverlapSaveFIR>>());
        firs.back()->initBuffers(kNumChannels, irSize, kFrameSize);
    }

    AudioBuffer in(1, kFrameSize);
    AudioBuffer output(kNumChannels, kFrameSize);
    std::vector<float> outputs;
    std::vector<int32_t> states;

    for (const auto& frame : kMixerFrames)
    {
        for (int e = 0; e < 2; ++e)
        {
            const char op = frame.ops[e];
            AudioEffectState state;
            if (op == 'U' || op == 'A')
            {
                if (op == 'U')
                {
                    fill(lcg, *firs[e]->writeBuffer);
                    firs[e]->commitWriteBuffer();
                }
                fill(lcg, in);

                OverlapSaveConvolutionEffectParams params{};
                params.fftIR = firs[e].get();
                params.numChannels = kNumChannels;
                params.numSamples = kIRSizes[e];
                state = effects[e]->apply(params, in, mixer);
            }
            else if (op == 'T')
            {
                state = effects[e]->tail(mixer);
            }
            else
            {
                states.push_back(-1);
                continue;
            }
            states.push_back(state == AudioEffectState::TailRemaining ? 0 : 1);
        }

        OverlapSaveConvolutionMixerParams params{};
        params.numChannels = frame.mixChannels;
        mixer.apply(params, output);
        for (int i = 0; i < kNumChannels; ++i)
            outputs.insert(outputs.end(), output[i], output[i] + kFrameSize);
    }

    writeNpy(out / "mixer.npy", {std::size(kMixerFrames), static_cast<size_t>(kNumChannels), static_cast<size_t>(kFrameSize)},
             outputs.data());
    writeNpy(out / "mixer_states.npy", {std::size(kMixerFrames), 2}, states.data());
}

void generateConvolution(const std::filesystem::path& out)
{
    generateOverlapAdd(out);
    generatePartitioner(out);
    generateOverlapSave(out);
    generateMixer(out);
}

Register convolution("convolution", generateConvolution);

}
