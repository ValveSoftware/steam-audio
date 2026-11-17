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

// Checks the binaural Ambisonics decode (AmbisonicsBinauralEffect + the Ambisonics HRTFs of HRTFDatabase)
// against direct HRTF rendering (HRTFDatabase::interpolatedHRTF): diffuse-field magnitude, diffuse-field
// interaural coherence, low-frequency ITD, ILD and front/back spectral cues.
//
// The decoded transfer function for a plane wave from direction d is sum_c Y_c(d) G_c(f), where G_c is the
// impulse response of the effect to an impulse in Ambisonics channel c (this includes any weighting the effect
// applies at run time). The evaluation grid (600-point Fibonacci sphere) is not used by the projection itself.

#include <catch.hpp>

#include <complex>
#include <cstdio>
#include <vector>

#include <ambisonics_binaural_effect.h>
#include <audio_buffer.h>
#include <fft.h>
#include <hrtf_database.h>
#include <sh.h>

namespace {

using cvec = std::vector<std::complex<double>>;

const int kSR = 48000;
const int kFrame = 1024;
const int kIRLen = 2048;
const double kPiD = 3.14159265358979323846;
const double kOct[] = { 125, 250, 500, 1000, 2000, 4000, 8000 };
const int kNumOct = 7;

std::vector<ipl::Vector3f> fibonacciSphere(int n)
{
    std::vector<ipl::Vector3f> d;
    const double ga = kPiD * (3.0 - std::sqrt(5.0));
    for (int i = 0; i < n; ++i)
    {
        double y = 1.0 - 2.0 * (i + 0.5) / n;
        double r = std::sqrt(1.0 - y * y);
        d.emplace_back((float) (r * std::cos(ga * i)), (float) y, (float) (r * std::sin(ga * i)));
    }
    return d;
}

struct Decoder
{
    int order = 0;
    std::vector<cvec> G[2]; // [ear][channel] spectrum, kIRLen / 2 + 1 bins.
};

// Measures the per-channel binaural filters of the effect, as heard at its output.
Decoder measureDecoder(const ipl::HRTFDatabase& db, int order)
{
    Decoder dec;
    dec.order = order;
    const int numChannels = ipl::SphericalHarmonics::numCoeffsForOrder(order);

    ipl::AudioSettings audioSettings{ kSR, kFrame };
    ipl::AmbisonicsBinauralEffectSettings settings{};
    settings.maxOrder = order;
    settings.hrtf = &db;
    ipl::AmbisonicsBinauralEffect effect(audioSettings, settings);

    ipl::AudioBuffer in(numChannels, kFrame);
    ipl::AudioBuffer out(2, kFrame);
    ipl::FFT fft(kIRLen);
    std::vector<float> ir(fft.numRealSamples);
    std::vector<ipl::complex_t> spectrum(fft.numComplexSamples);

    for (int e = 0; e < 2; ++e)
        dec.G[e].resize(numChannels);

    for (int c = 0; c < numChannels; ++c)
    {
        effect.reset();
        std::vector<float> L, R;
        for (int b = 0; b < kIRLen / kFrame; ++b)
        {
            in.makeSilent();
            if (b == 0)
                in[c][0] = 1.0f;

            ipl::AmbisonicsBinauralEffectParams params{};
            params.hrtf = &db;
            params.order = order;
            effect.apply(params, in, out);

            L.insert(L.end(), out[0], out[0] + kFrame);
            R.insert(R.end(), out[1], out[1] + kFrame);
        }

        for (int e = 0; e < 2; ++e)
        {
            const auto& x = (e == 0) ? L : R;
            std::fill(ir.begin(), ir.end(), 0.0f);
            std::copy(x.begin(), x.end(), ir.begin());
            fft.applyForward(ir.data(), spectrum.data());
            dec.G[e][c].assign(spectrum.begin(), spectrum.end());
        }
    }

    return dec;
}

void decodedTF(const Decoder& dec, const ipl::Vector3f& dir, cvec& L, cvec& R)
{
    const size_t nb = dec.G[0][0].size();
    L.assign(nb, 0.0);
    R.assign(nb, 0.0);
    for (int l = 0, c = 0; l <= dec.order; ++l)
    {
        for (int m = -l; m <= l; ++m, ++c)
        {
            double y = ipl::SphericalHarmonics::evaluate(l, m, dir);
            for (size_t k = 0; k < nb; ++k)
            {
                L[k] += y * dec.G[0][c][k];
                R[k] += y * dec.G[1][c][k];
            }
        }
    }
}

void directTF(ipl::HRTFDatabase& db, const ipl::Vector3f& dir, cvec& L, cvec& R)
{
    const int nb = db.numSpectrumSamples();
    std::vector<ipl::complex_t> a(nb), b(nb);
    ipl::complex_t* out[2] = { a.data(), b.data() };
    db.interpolatedHRTF(dir, out, 1.0f, ipl::HRTFPhaseType::None);
    L.assign(a.begin(), a.end());
    R.assign(b.begin(), b.end());
}

double binFreq(int k, size_t nb)
{
    return (double) k * kSR / (2.0 * (nb - 1));
}

bool inOctave(double f, int o)
{
    return f >= kOct[o] / std::sqrt(2.0) && f < kOct[o] * std::sqrt(2.0);
}

double bandPower(const cvec& X, double lo, double hi)
{
    double s = 0.0;
    for (size_t k = 0; k < X.size(); ++k)
    {
        double f = binFreq((int) k, X.size());
        if (f >= lo && f < hi)
            s += std::norm(X[k]);
    }
    return s;
}

double octavePower(const cvec& X, int o)
{
    return bandPower(X, kOct[o] / std::sqrt(2.0), kOct[o] * std::sqrt(2.0));
}

// Interaural group delay (left re right, seconds) from the slope of the cross-spectrum phase over [lo, hi).
double interauralDelay(const cvec& L, const cvec& R, double lo, double hi)
{
    std::vector<double> w, ph;
    double prev = 0.0;
    for (size_t k = 0; k < L.size(); ++k)
    {
        double f = binFreq((int) k, L.size());
        if (f < lo || f >= hi)
            continue;
        double a = std::arg(L[k] * std::conj(R[k]));
        if (!ph.empty())
        {
            while (a - prev > kPiD) a -= 2.0 * kPiD;
            while (a - prev < -kPiD) a += 2.0 * kPiD;
        }
        prev = a;
        w.push_back(2.0 * kPiD * f);
        ph.push_back(a);
    }
    double mw = 0.0, mp = 0.0;
    for (size_t i = 0; i < w.size(); ++i) { mw += w[i]; mp += ph[i]; }
    mw /= w.size(); mp /= w.size();
    double num = 0.0, den = 0.0;
    for (size_t i = 0; i < w.size(); ++i) { num += (w[i] - mw) * (ph[i] - mp); den += (w[i] - mw) * (w[i] - mw); }
    return -num / den;
}

struct Metrics
{
    double df[kNumOct];        // Diffuse-field power per octave, both ears.
    double ic[kNumOct];        // Diffuse-field interaural coherence per octave (mean |coh| over bins).
    double itd90;              // Interaural delay for a source at the right, 200-1000 Hz, seconds.
    double itd45;              // Same, 45 deg front-right.
    double ild90[kNumOct];     // ILD (left re right, dB) for a source at the right.
    double frontBack[kNumOct]; // Front re back level (dB), both ears, horizontal plane.
};

template <typename TF>
Metrics measure(TF tf)
{
    Metrics m{};
    auto dirs = fibonacciSphere(600);
    cvec L, R;
    tf(dirs[0], L, R);
    const size_t nb = L.size();
    cvec cross(nb, 0.0);
    std::vector<double> pl(nb, 0.0), pr(nb, 0.0);
    for (const auto& d : dirs)
    {
        tf(d, L, R);
        for (size_t k = 0; k < nb; ++k)
        {
            cross[k] += L[k] * std::conj(R[k]);
            pl[k] += std::norm(L[k]);
            pr[k] += std::norm(R[k]);
        }
    }
    for (int o = 0; o < kNumOct; ++o)
    {
        double p = 0.0, c = 0.0;
        int n = 0;
        for (size_t k = 0; k < nb; ++k)
        {
            if (!inOctave(binFreq((int) k, nb), o))
                continue;
            p += pl[k] + pr[k];
            c += std::abs(cross[k]) / std::sqrt(pl[k] * pr[k]);
            ++n;
        }
        m.df[o] = p;
        m.ic[o] = c / n;
    }

    tf(ipl::Vector3f(1.0f, 0.0f, 0.0f), L, R);
    m.itd90 = interauralDelay(L, R, 200.0, 1000.0);
    for (int o = 0; o < kNumOct; ++o)
        m.ild90[o] = 10.0 * std::log10(octavePower(L, o) / octavePower(R, o));

    tf(ipl::Vector3f(0.70710678f, 0.0f, -0.70710678f), L, R);
    m.itd45 = interauralDelay(L, R, 200.0, 1000.0);

    cvec Lb, Rb;
    tf(ipl::Vector3f(0.0f, 0.0f, -1.0f), L, R);
    tf(ipl::Vector3f(0.0f, 0.0f, 1.0f), Lb, Rb);
    for (int o = 0; o < kNumOct; ++o)
        m.frontBack[o] = 10.0 * std::log10((octavePower(L, o) + octavePower(R, o)) / (octavePower(Lb, o) + octavePower(Rb, o)));

    return m;
}

Metrics measureDirect(ipl::HRTFDatabase& db)
{
    return measure([&](const ipl::Vector3f& d, cvec& L, cvec& R) { directTF(db, d, L, R); });
}

Metrics measureDecoded(const Decoder& dec)
{
    return measure([&](const ipl::Vector3f& d, cvec& L, cvec& R) { decodedTF(dec, d, L, R); });
}

// RMS over the evaluation grid, both ears and the 1-8 kHz octaves, of the per-direction level error (dB).
double directionalLevelErrorDB(ipl::HRTFDatabase& db, const Decoder& dec)
{
    auto dirs = fibonacciSphere(600);
    cvec L, R, Ld, Rd;
    double s = 0.0;
    int n = 0;
    for (const auto& d : dirs)
    {
        directTF(db, d, L, R);
        decodedTF(dec, d, Ld, Rd);
        for (int o = 3; o < kNumOct; ++o)
        {
            double el = 10.0 * std::log10(octavePower(Ld, o) / octavePower(L, o));
            double er = 10.0 * std::log10(octavePower(Rd, o) / octavePower(R, o));
            s += el * el + er * er;
            n += 2;
        }
    }
    return std::sqrt(s / n);
}

void printRow(const char* name, const double* v, const char* fmt)
{
    printf("    %-24s", name);
    for (int o = 0; o < kNumOct; ++o)
        printf(fmt, v[o]);
    printf("\n");
}

struct Report
{
    Metrics direct;
    Metrics decoded[4];
    double dirErr[4];
};

Report evaluate(const char* name, ipl::HRTFDatabase& db)
{
    Report r{};
    r.direct = measureDirect(db);
    for (int order = 0; order <= 3; ++order)
    {
        auto dec = measureDecoder(db, order);
        r.decoded[order] = measureDecoded(dec);
        r.dirErr[order] = directionalLevelErrorDB(db, dec);
    }

    printf("\n[%s] octaves:", name);
    for (int o = 0; o < kNumOct; ++o)
        printf(" %7.0f", kOct[o]);
    printf("\n  diffuse field re direct (dB):\n");
    for (int order = 0; order <= 3; ++order)
    {
        double v[kNumOct];
        for (int o = 0; o < kNumOct; ++o)
            v[o] = 10.0 * std::log10(r.decoded[order].df[o] / r.direct.df[o]);
        char label[32];
        snprintf(label, sizeof(label), "order %d", order);
        printRow(label, v, " %7.2f");
    }
    printf("  diffuse-field interaural coherence:\n");
    printRow("direct", r.direct.ic, " %7.2f");
    for (int order = 1; order <= 3; ++order)
    {
        char label[32];
        snprintf(label, sizeof(label), "order %d", order);
        printRow(label, r.decoded[order].ic, " %7.2f");
    }
    printf("  ILD at 90 deg (dB):\n");
    printRow("direct", r.direct.ild90, " %7.1f");
    for (int order = 1; order <= 3; ++order)
    {
        char label[32];
        snprintf(label, sizeof(label), "order %d", order);
        printRow(label, r.decoded[order].ild90, " %7.1f");
    }
    printf("  front re back level (dB):\n");
    printRow("direct", r.direct.frontBack, " %7.1f");
    for (int order = 1; order <= 3; ++order)
    {
        char label[32];
        snprintf(label, sizeof(label), "order %d", order);
        printRow(label, r.decoded[order].frontBack, " %7.1f");
    }
    printf("  ITD 200-1000 Hz (us): direct 90deg %.0f 45deg %.0f\n", 1e6 * r.direct.itd90, 1e6 * r.direct.itd45);
    for (int order = 1; order <= 3; ++order)
        printf("    order %d: 90deg %.0f (%.0f %%), 45deg %.0f (%.0f %%)\n", order,
               1e6 * r.decoded[order].itd90, 100.0 * r.decoded[order].itd90 / r.direct.itd90,
               1e6 * r.decoded[order].itd45, 100.0 * r.decoded[order].itd45 / r.direct.itd45);
    printf("  per-direction level error, RMS over directions/ears/1-8 kHz octaves (dB):");
    for (int order = 0; order <= 3; ++order)
        printf(" o%d %.2f", order, r.dirErr[order]);
    printf("\n");
    return r;
}

const Report& defaultReport()
{
    static Report r = [] {
        ipl::HRTFSettings s{};
        s.type = ipl::HRTFMapType::Default;
        ipl::HRTFDatabase db(s, kSR, kFrame);
        return evaluate("default HRTF", db);
    }();
    return r;
}

const Report& sofaReport()
{
    static Report r = [] {
        ipl::HRTFSettings s{};
        s.type = ipl::HRTFMapType::SOFA;
        s.sofaFileName = "../../data/hrtf/cipic_124.sofa";
        ipl::HRTFDatabase db(s, kSR, kFrame);
        return evaluate("cipic_124.sofa", db);
    }();
    return r;
}

void checkDiffuseField(const Report& r)
{
    for (int order : { 1, 3 })
    {
        for (int o = 0; o < kNumOct; ++o)
        {
            INFO("order " << order << ", octave " << kOct[o] << " Hz");
            double dB = 10.0 * std::log10(r.decoded[order].df[o] / r.direct.df[o]);
            CHECK(std::abs(dB) <= 1.5);
        }
    }
}

void checkCoherence(const Report& r)
{
    for (int order : { 1, 3 })
    {
        for (int o = 3; o < kNumOct; ++o)
        {
            INFO("order " << order << ", octave " << kOct[o] << " Hz, direct " << r.direct.ic[o] << ", decoded " << r.decoded[order].ic[o]);
            CHECK(std::abs(r.decoded[order].ic[o] - r.direct.ic[o]) <= 0.15);
        }
    }
}

}

TEST_CASE("Binaural Ambisonics decode keeps the HRTF diffuse-field response (default HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkDiffuseField(defaultReport());
}

TEST_CASE("Binaural Ambisonics decode keeps the HRTF diffuse-field response (SOFA HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkDiffuseField(sofaReport());
}

TEST_CASE("Binaural Ambisonics decode keeps the HRTF diffuse-field interaural coherence (default HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkCoherence(defaultReport());
}

TEST_CASE("Binaural Ambisonics decode keeps the HRTF diffuse-field interaural coherence (SOFA HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkCoherence(sofaReport());
}
