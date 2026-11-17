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

#include <chrono>
#include <complex>
#include <cstdint>
#include <cstring>
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

// Interaural delay (left re right, seconds) as the lag of the maximum of the interaural cross-correlation of the
// responses low-passed at 1.5 kHz, searched within +-1 ms in 1 us steps.
double interauralDelayXcorr(const cvec& L, const cvec& R)
{
    double best = -1e300, bestLag = 0.0;
    for (int t = -1000; t <= 1000; ++t)
    {
        double lag = t * 1e-6, r = 0.0;
        for (size_t k = 1; k < L.size(); ++k)
        {
            double f = binFreq((int) k, L.size());
            if (f >= 1500.0)
                break;
            r += std::real(L[k] * std::conj(R[k]) * std::polar(1.0, 2.0 * kPiD * f * lag));
        }
        if (r > best) { best = r; bestLag = lag; }
    }
    return bestLag;
}

struct Metrics
{
    double df[kNumOct];        // Diffuse-field power per octave, both ears.
    double ic[kNumOct];        // Diffuse-field interaural coherence per octave (mean |coh| over bins).
    double itd90;              // Interaural delay for a source at the right, 200-1000 Hz, seconds.
    double itd45;              // Same, 45 deg front-right.
    double xitd90;             // Interaural delay from the cross-correlation below 1.5 kHz, source at the right.
    double xitd45;             // Same, 45 deg front-right.
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
    m.xitd90 = interauralDelayXcorr(L, R);
    for (int o = 0; o < kNumOct; ++o)
        m.ild90[o] = 10.0 * std::log10(octavePower(L, o) / octavePower(R, o));

    tf(ipl::Vector3f(0.70710678f, 0.0f, -0.70710678f), L, R);
    m.itd45 = interauralDelay(L, R, 200.0, 1000.0);
    m.xitd45 = interauralDelayXcorr(L, R);

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

// RMS over 36 horizontal-plane directions and the 1-8 kHz octaves of the ILD error re direct rendering (dB).
double horizontalILDErrorDB(ipl::HRTFDatabase& db, const Decoder& dec)
{
    cvec L, R, Ld, Rd;
    double s = 0.0;
    int n = 0;
    for (int a = 0; a < 36; ++a)
    {
        double az = a * 10.0 * kPiD / 180.0;
        ipl::Vector3f d((float) std::sin(az), 0.0f, (float) -std::cos(az));
        directTF(db, d, L, R);
        decodedTF(dec, d, Ld, Rd);
        for (int o = 3; o < kNumOct; ++o)
        {
            double e = 10.0 * std::log10(octavePower(Ld, o) / octavePower(Rd, o)) - 10.0 * std::log10(octavePower(L, o) / octavePower(R, o));
            s += e * e;
            ++n;
        }
    }
    return std::sqrt(s / n);
}

// RMS of the error re direct rendering of the front/back level difference (dB), per ear, over 9 pairs of
// horizontal directions mirrored about the interaural axis (azimuth a and 180 - a, a = 0..80 deg), 1-8 kHz octaves.
double frontBackErrorDB(ipl::HRTFDatabase& db, const Decoder& dec)
{
    auto horizontal = [](double deg) {
        double az = deg * kPiD / 180.0;
        return ipl::Vector3f((float) std::sin(az), 0.0f, (float) -std::cos(az));
    };
    cvec Lf, Rf, Lb, Rb, Lfd, Rfd, Lbd, Rbd;
    double s = 0.0;
    int n = 0;
    for (int a = 0; a <= 80; a += 10)
    {
        directTF(db, horizontal(a), Lf, Rf);
        directTF(db, horizontal(180 - a), Lb, Rb);
        decodedTF(dec, horizontal(a), Lfd, Rfd);
        decodedTF(dec, horizontal(180 - a), Lbd, Rbd);
        for (int o = 3; o < kNumOct; ++o)
        {
            double el = 10.0 * std::log10(octavePower(Lfd, o) / octavePower(Lbd, o)) - 10.0 * std::log10(octavePower(Lf, o) / octavePower(Lb, o));
            double er = 10.0 * std::log10(octavePower(Rfd, o) / octavePower(Rbd, o)) - 10.0 * std::log10(octavePower(Rf, o) / octavePower(Rb, o));
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
    double hildErr[4];
    double fbErr[4];
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
        r.hildErr[order] = horizontalILDErrorDB(db, dec);
        r.fbErr[order] = frontBackErrorDB(db, dec);
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
    printf("  ITD cross-correlation < 1.5 kHz (us): direct 90deg %.0f 45deg %.0f\n", 1e6 * r.direct.xitd90, 1e6 * r.direct.xitd45);
    for (int order = 1; order <= 3; ++order)
        printf("    order %d: 90deg %.0f (%.0f %%), 45deg %.0f (%.0f %%)\n", order,
               1e6 * r.decoded[order].xitd90, 100.0 * r.decoded[order].xitd90 / r.direct.xitd90,
               1e6 * r.decoded[order].xitd45, 100.0 * r.decoded[order].xitd45 / r.direct.xitd45);
    printf("  per-direction level error, RMS over directions/ears/1-8 kHz octaves (dB):");
    for (int order = 0; order <= 3; ++order)
        printf(" o%d %.2f", order, r.dirErr[order]);
    printf("\n  horizontal-plane ILD error, RMS over 36 azimuths and 1-8 kHz octaves (dB):");
    for (int order = 0; order <= 3; ++order)
        printf(" o%d %.2f", order, r.hildErr[order]);
    printf("\n  front/back level difference error, RMS over 9 mirrored pairs, ears, 1-8 kHz octaves (dB):");
    for (int order = 0; order <= 3; ++order)
        printf(" o%d %.2f", order, r.fbErr[order]);
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

// Interaural time difference of the decode, as a fraction of that of direct rendering. Order N represents the
// interaural phase only up to about f_N = N c / (2 pi r) (624 Hz per order for r = 8.75 cm; Zotter & Frank,
// "Ambisonics", eq. 4.53). Below the MagLS cutoff the filters are an ordinary least-squares fit, so order 3
// (f_3 = 1.87 kHz) must keep the ITD of the 1.5 kHz band, while order 1 is limited to its least-squares truncation
// (about half of the ITD of a lateral source): there the requirement is no regression re the previous projection
// (48 % at 90 deg, 52 % at 45 deg).
void checkITD(const Report& r)
{
    INFO("direct " << r.direct.xitd90 * 1e6 << " / " << r.direct.xitd45 * 1e6 << " us; order 1 "
         << r.decoded[1].xitd90 * 1e6 << " / " << r.decoded[1].xitd45 * 1e6 << " us; order 3 "
         << r.decoded[3].xitd90 * 1e6 << " / " << r.decoded[3].xitd45 * 1e6 << " us");
    CHECK(r.decoded[3].xitd90 >= 0.85 * r.direct.xitd90);
    CHECK(r.decoded[3].xitd45 >= 0.85 * r.direct.xitd45);
    CHECK(r.decoded[1].xitd90 >= 0.45 * r.direct.xitd90);
    CHECK(r.decoded[1].xitd45 >= 0.50 * r.direct.xitd45);
}

// ILD and front/back cues. Order 3 must follow the HRTF (the previous projection: 3.4-3.6 dB ILD error, 3.1-3.3 dB
// front/back error); order 1 must not regress (previous: 5.7-6.0 dB and 3.4-3.9 dB).
void checkILDAndFrontBack(const Report& r)
{
    INFO("ILD error o1 " << r.hildErr[1] << " o3 " << r.hildErr[3] << " dB; front/back error o1 " << r.fbErr[1] << " o3 " << r.fbErr[3] << " dB");
    CHECK(r.hildErr[3] <= 3.0);
    CHECK(r.fbErr[3] <= 2.5);
    CHECK(r.hildErr[1] <= 6.2);
    CHECK(r.fbErr[1] <= 4.0);
}

// Maximum relative difference between the Ambisonics HRTFs (order 3 set) of two databases.
double ambisonicsHRTFDifference(const ipl::HRTFDatabase& a, const ipl::HRTFDatabase& b)
{
    double num = 0.0, den = 0.0;
    for (int c = 0; c < 16; ++c)
    {
        const ipl::complex_t* x[2] = { nullptr, nullptr };
        const ipl::complex_t* y[2] = { nullptr, nullptr };
        a.ambisonicsHRTF(c, x);
        b.ambisonicsHRTF(c, y);
        for (int e = 0; e < 2; ++e)
        {
            for (int k = 0; k < a.numSpectrumSamples(); ++k)
            {
                num += std::norm(x[e][k] - y[e][k]);
                den += std::norm(x[e][k]);
            }
        }
    }
    return std::sqrt(num / den);
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

TEST_CASE("Binaural Ambisonics decode keeps the low-frequency ITD (default HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkITD(defaultReport());
}

TEST_CASE("Binaural Ambisonics decode keeps the low-frequency ITD (SOFA HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkITD(sofaReport());
}

TEST_CASE("Binaural Ambisonics decode keeps ILD and front/back cues (default HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkILDAndFrontBack(defaultReport());
}

TEST_CASE("Binaural Ambisonics decode keeps ILD and front/back cues (SOFA HRTF).", "[AmbisonicsHRTFProjection]")
{
    checkILDAndFrontBack(sofaReport());
}

TEST_CASE("Ambisonics HRTFs are deterministic.", "[AmbisonicsHRTFProjection]")
{
    ipl::HRTFSettings s{};
    s.type = ipl::HRTFMapType::SOFA;
    s.sofaFileName = "../../data/hrtf/cipic_124.sofa";
    ipl::HRTFDatabase a(s, kSR, kFrame);
    ipl::HRTFDatabase b(s, kSR, kFrame);
    REQUIRE(ambisonicsHRTFDifference(a, b) == 0.0);
}

extern ipl::byte_t gDefaultHrtfData[];

// The Ambisonics HRTFs shipped with the default HRTF must be those the current code computes from its HRIRs
// (catches stale data after a change to the design). The HRIR part is copied with format version 2 (no
// Ambisonics data), which makes HRTFDatabase compute the filters.
TEST_CASE("Shipped Ambisonics HRTFs match the in-process design.", "[AmbisonicsHRTFProjection]")
{
    auto read = [](const uint8_t*& q) { int32_t v; memcpy(&v, q, 4); q += 4; return v; };
    const uint8_t* base = reinterpret_cast<const uint8_t*>(gDefaultHrtfData);
    const uint8_t* q = base + 8;
    int numHRIRs = read(q);
    q += 2 * (size_t) numHRIRs * sizeof(float);
    int numRates = read(q);
    std::vector<int> lengths(numRates);
    for (int i = 0; i < numRates; ++i) { read(q); lengths[i] = read(q); }
    for (int i = 0; i < numRates; ++i)
        q += (size_t) 2 * numHRIRs * lengths[i] * sizeof(float);
    std::vector<uint8_t> hrirOnly(base, q);
    int32_t version2 = 2;
    memcpy(hrirOnly.data() + 4, &version2, 4);

    for (int rate : { 44100, 48000, 24000 })
    {
        ipl::HRTFSettings shipped{};
        ipl::HRTFSettings computed{};
        computed.hrtfData = hrirOnly.data();
        ipl::HRTFDatabase a(shipped, rate, kFrame);
        ipl::HRTFDatabase b(computed, rate, kFrame);
        double diff = ambisonicsHRTFDifference(a, b);
        INFO(rate << " Hz: relative difference " << diff);
        CHECK(diff < 1e-3);
    }
}

// Load time (hidden): construction time of HRTFDatabase, which designs the Ambisonics HRTFs when the data has none.
TEST_CASE("HRTF load time.", "[.hrtf-load-time]")
{
    struct Case { const char* name; ipl::HRTFMapType type; const char* file; };
    const Case cases[] = {
        { "default", ipl::HRTFMapType::Default, nullptr },
        { "cipic_124.sofa", ipl::HRTFMapType::SOFA, "../../data/hrtf/cipic_124.sofa" },
        { "sadie_d1.sofa", ipl::HRTFMapType::SOFA, "../../data/hrtf/sadie_d1.sofa" },
        { "sadie_h12.sofa", ipl::HRTFMapType::SOFA, "../../data/hrtf/sadie_h12.sofa" },
    };
    for (const auto& c : cases)
    {
        for (int rate : { 44100, 48000 })
        {
            ipl::HRTFSettings s{};
            s.type = c.type;
            s.sofaFileName = c.file;
            const int runs = 5;
            double best = 1e30;
            int numHRIRs = 0, numSamples = 0;
            for (int i = 0; i < runs; ++i)
            {
                auto t0 = std::chrono::high_resolution_clock::now();
                ipl::HRTFDatabase db(s, rate, kFrame);
                auto t1 = std::chrono::high_resolution_clock::now();
                best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
                numHRIRs = db.numHRIRs();
                numSamples = db.numSamples();
            }
            printf("  %-16s %d Hz (%d HRIRs x %d samples): %.1f ms (best of %d)\n", c.name, rate, numHRIRs, numSamples, best, runs);
        }
    }
}
