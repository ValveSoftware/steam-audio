// Golden data for stage 2.7: bands.{h,cpp}, iir.{h,cpp} (+ float8_iir.cpp), delay.{h,cpp}, window_function.{h,cpp}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/dsp.rs replays the
// same order.
//
// IIR2Filterer picks its block algorithm (4 or 8 samples per step) from Context::sSIMDLevel when it is constructed.
// The generator never creates a Context, so the level is set directly to produce both variants. The AVX variant
// needs a CPU with AVX.

#include <cstring>

#include <bands.h>
#include <context.h>
#include <delay.h>
#include <float4.h>
#include <iir.h>
#include <window_function.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

void generateBands(const std::filesystem::path& out)
{
    const std::vector<float> low(Bands::kLowCutoffFrequencies, Bands::kLowCutoffFrequencies + Bands::kNumBands);
    const std::vector<float> high(Bands::kHighCutoffFrequencies, Bands::kHighCutoffFrequencies + Bands::kNumBands);
    const std::vector<float> norm(Bands::kBandPassNorm, Bands::kBandPassNorm + Bands::kNumBands);
    writeNpy(out / "low_cutoff.npy", low);
    writeNpy(out / "high_cutoff.npy", high);
    writeNpy(out / "band_pass_norm.npy", norm);
}

// --------------------------------------------------------------------------------------------------------------------
// IIR
// --------------------------------------------------------------------------------------------------------------------

constexpr int kSamplingRates[] = {44100, 48000, 24000};
constexpr int kDesignsPerRate = 6;
constexpr int kNumSpectrumFrequencies = 9;

void push(std::vector<float>& rows, const IIR2& f)
{
    rows.push_back(f.a1);
    rows.push_back(f.a2);
    rows.push_back(f.b0);
    rows.push_back(f.b1);
    rows.push_back(f.b2);
}

// Ten designs per parameter set, in this order: lowPass, lowPass(Q), highPass, highPass(Q), bandPass, lowShelf,
// lowShelf(Q), highShelf, highShelf(Q), peaking.
std::vector<IIR2> designs(Lcg& lcg)
{
    std::vector<IIR2> filters;
    for (int samplingRate : kSamplingRates)
    {
        for (int k = 0; k < kDesignsPerRate; ++k)
        {
            const float u = lcg.nextSigned();
            const float cutoff = 20.0f + 5000.0f * (u + 1.0f);
            const float v = lcg.nextSigned();
            const float q = 0.3f + 2.0f * (v + 1.0f);
            const float w = lcg.nextSigned();
            const float gain = 0.05f + 2.0f * (w + 1.0f);
            const float a = lcg.nextSigned();
            const float low = 50.0f + 2000.0f * (a + 1.0f);
            const float b = lcg.nextSigned();
            const float high = low * (1.5f + 2.0f * (b + 1.0f));

            filters.push_back(IIR2::lowPass(cutoff, samplingRate));
            filters.push_back(IIR2::lowPass(cutoff, q, samplingRate));
            filters.push_back(IIR2::highPass(cutoff, samplingRate));
            filters.push_back(IIR2::highPass(cutoff, q, samplingRate));
            filters.push_back(IIR2::bandPass(low, high, samplingRate));
            filters.push_back(IIR2::lowShelf(cutoff, gain, samplingRate));
            filters.push_back(IIR2::lowShelf(cutoff, q, gain, samplingRate));
            filters.push_back(IIR2::highShelf(cutoff, gain, samplingRate));
            filters.push_back(IIR2::highShelf(cutoff, q, gain, samplingRate));
            filters.push_back(IIR2::peaking(low, high, gain, samplingRate));
        }
    }
    return filters;
}

// Frame sizes for one filtering run: SIMD tails, frames shorter than a block, and a single-sample frame.
constexpr int kFrames[] = {37, 64, 3, 8, 1, 100};
constexpr int kNumSingles = 5;
constexpr int kRunLength = 300;
constexpr int kCopyLength = 40;
constexpr int kResetLength = 40;

std::vector<float> noise(Lcg& lcg, int n)
{
    std::vector<float> x(n);
    for (auto& v : x)
        v = lcg.nextSigned();
    return x;
}

// Block frames, then single samples, then the rest as one frame; then a fresh filterer continues from copied state;
// then the original filterer is reset and run again. Returns kRunLength + kCopyLength + kResetLength samples.
std::vector<float> filterRun(const IIR2& filter, SIMDLevel level, uint32_t seed)
{
    Context::sSIMDLevel = level;
    IIR2Filterer f;
    IIR2Filterer g;
    Context::sSIMDLevel = SIMDLevel::SSE2;

    f.setFilter(filter);
    g.setFilter(filter);

    Lcg lcg(seed);
    const auto x = noise(lcg, kRunLength + kCopyLength + kResetLength);
    std::vector<float> y(x.size());

    int pos = 0;
    for (int n : kFrames)
    {
        f.apply(n, &x[pos], &y[pos]);
        pos += n;
    }
    for (int i = 0; i < kNumSingles; ++i, ++pos)
        y[pos] = f.apply(x[pos]);
    f.apply(kRunLength - pos, &x[pos], &y[pos]);
    pos = kRunLength;

    g.copyState(f);
    g.apply(kCopyLength, &x[pos], &y[pos]);
    pos += kCopyLength;

    f.reset();
    f.apply(kResetLength, &x[pos], &y[pos]);

    return y;
}

void generateIir(const std::filesystem::path& out)
{
    Lcg lcg(27);
    const auto filters = designs(lcg);

    std::vector<float> coeffs;
    for (const auto& f : filters)
        push(coeffs, f);
    writeNpy(out / "designs.npy", {filters.size(), 5}, coeffs.data());

    std::vector<float> spectrum;
    for (auto f : filters)
        for (int j = 0; j < kNumSpectrumFrequencies; ++j)
            spectrum.push_back(f.spectrum(j * 0.39f));
    writeNpy(out / "spectrum.npy", {filters.size(), static_cast<size_t>(kNumSpectrumFrequencies)}, spectrum.data());

    std::vector<float> bands;
    for (int samplingRate : kSamplingRates)
    {
        IIR2 band[Bands::kNumBands];
        IIR2::bandFilters(band, samplingRate);
        for (const auto& f : band)
            push(bands, f);

        // IIR::bandFilters only differs in the (unused) 8th-order part.
        IIR wrapped[Bands::kNumBands];
        IIR::bandFilters(wrapped, samplingRate);
        for (int i = 0; i < Bands::kNumBands; ++i)
        {
            if (memcmp(&wrapped[i].m_iir2, &band[i], sizeof(IIR2)) != 0)
                throw std::runtime_error("IIR::bandFilters differs from IIR2::bandFilters");
        }
    }
    writeNpy(out / "band_filters.npy", {std::size(kSamplingRates), static_cast<size_t>(Bands::kNumBands), 5},
             bands.data());

    // Filters for the runs: the 44.1 kHz band filters, then the shelf/peaking filters ReverbEffect and EQEffect build,
    // then one of each random design.
    std::vector<IIR2> runFilters(Bands::kNumBands);
    IIR2::bandFilters(runFilters.data(), 44100);
    runFilters.push_back(IIR2::lowShelf(Bands::kHighCutoffFrequencies[0], 0.3f, 44100));
    runFilters.push_back(IIR2::peaking(Bands::kLowCutoffFrequencies[1], Bands::kHighCutoffFrequencies[1], 2.5f, 44100));
    runFilters.push_back(IIR2::highShelf(Bands::kLowCutoffFrequencies[2], 0.7f, 48000));
    runFilters.insert(runFilters.end(), filters.begin(), filters.begin() + 10);

    const size_t runSize = kRunLength + kCopyLength + kResetLength;
    std::vector<float> runs4;
    std::vector<float> runs8;
    for (size_t i = 0; i < runFilters.size(); ++i)
    {
        const auto seed = static_cast<uint32_t>(100 + i);
        const auto y4 = filterRun(runFilters[i], SIMDLevel::SSE2, seed);
        const auto y8 = filterRun(runFilters[i], SIMDLevel::AVX, seed);
        runs4.insert(runs4.end(), y4.begin(), y4.end());
        runs8.insert(runs8.end(), y8.begin(), y8.end());
    }
    std::vector<float> runCoeffs;
    for (const auto& f : runFilters)
        push(runCoeffs, f);
    writeNpy(out / "run_filters.npy", {runFilters.size(), 5}, runCoeffs.data());
    writeNpy(out / "run_float4.npy", {runFilters.size(), runSize}, runs4.data());
    writeNpy(out / "run_float8.npy", {runFilters.size(), runSize}, runs8.data());
}

// --------------------------------------------------------------------------------------------------------------------
// Delay, Allpass
// --------------------------------------------------------------------------------------------------------------------

constexpr int kDelayFrames[] = {5, 5, 3, 7, 12, 1, 5, 5, 11, 2, 9};

std::vector<float> applyBlocks(Allpass& a, Lcg& lcg, int numBlocks)
{
    std::vector<float> y;
    for (int i = 0; i < numBlocks; ++i)
    {
        alignas(float4_t) float x[4];
        for (auto& v : x)
            v = lcg.nextSigned();
        alignas(float4_t) float r[4];
        float4::store(r, a.apply(float4::load(x)));
        y.insert(y.end(), r, r + 4);
    }
    return y;
}

std::vector<float> applySingles(Allpass& a, Lcg& lcg, int n)
{
    std::vector<float> y;
    for (int i = 0; i < n; ++i)
    {
        const float x = lcg.nextSigned();
        y.push_back(a.apply(x));
    }
    return y;
}

void append(std::vector<float>& to, const std::vector<float>& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

void generateDelay(const std::filesystem::path& out)
{
    Lcg lcg(37);

    // Delay of 7 with frame size 5: a ring of 12. Each step reads a frame, then writes one.
    std::vector<float> delayed;
    Delay d(7, 5);
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int n : kDelayFrames)
        {
            std::vector<float> y(n);
            d.get(n, y.data());
            append(delayed, y);

            std::vector<float> x(n);
            for (auto& v : x)
                v = lcg.nextSigned();
            d.put(n, x.data());
        }
        d.reset();
    }
    writeNpy(out / "delay.npy", delayed);

    // Ring of 16: blocks, single samples, blocks; then reset (which zeroes the gains as well), then blocks.
    std::vector<float> allpass;
    Allpass a;
    a.resize(13, 0.5f, 3);
    append(allpass, applyBlocks(a, lcg, 10));
    append(allpass, applySingles(a, lcg, 7));
    append(allpass, applyBlocks(a, lcg, 5));
    a.reset();
    append(allpass, applyBlocks(a, lcg, 10));
    writeNpy(out / "allpass.npy", allpass);

    // Ring of 2, shorter than a block: every block wraps twice.
    std::vector<float> shortRing;
    Allpass b;
    b.resize(2, 0.7f, 0);
    append(shortRing, applyBlocks(b, lcg, 5));
    append(shortRing, applySingles(b, lcg, 3));
    append(shortRing, applyBlocks(b, lcg, 2));
    writeNpy(out / "allpass_short.npy", shortRing);

    // As ReverbEffect sets it up (before its reset()).
    Allpass c;
    c.resize(225, 0.5f, 0);
    writeNpy(out / "allpass_reverb.npy", applyBlocks(c, lcg, 100));
}

// --------------------------------------------------------------------------------------------------------------------
// WindowFunction
// --------------------------------------------------------------------------------------------------------------------

constexpr int kWindowSizes[] = {1, 2, 5, 31, 64, 257};
constexpr int kTukey[][2] = {{64, 16}, {37, 9}, {5, 2}, {4, 1}, {3, 5}, {0, 3}};

void generateWindow(const std::filesystem::path& out)
{
    const std::pair<const char*, void (*)(int, float*)> windows[] = {
        {"rectangular", WindowFunction::rectangular},
        {"bartlett", WindowFunction::bartlett},
        {"hann", WindowFunction::hann},
        {"hamming", WindowFunction::hamming},
        {"blackman", WindowFunction::blackman},
        {"blackman_harris", WindowFunction::blackmanHarris},
    };

    for (const auto& [name, window] : windows)
    {
        std::vector<float> all;
        for (int size : kWindowSizes)
        {
            std::vector<float> w(size);
            window(size, w.data());
            append(all, w);
        }
        writeNpy(out / (std::string(name) + ".npy"), all);
    }

    std::vector<float> tukey;
    for (const auto& [size, overlap] : kTukey)
    {
        std::vector<float> w(size + overlap);
        WindowFunction::tukey(size, overlap, w.data());
        append(tukey, w);
    }
    writeNpy(out / "tukey.npy", tukey);
}

Register bands("bands", generateBands);
Register iir("iir", generateIir);
Register delay("delay", generateDelay);
Register window("window", generateWindow);

}
