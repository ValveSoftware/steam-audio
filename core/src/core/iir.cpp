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

#include "iir.h"

#include "bands.h"
#include "context.h"
#include "math_functions.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IIR2
// --------------------------------------------------------------------------------------------------------------------

float IIR2::spectrum(float frequency)
{
    auto numerator = b0*b0 + b1*b1 + b2*b2 + 2.0f*(b0*b1 + b1*b2)*cosf(frequency) + 2.0f*b0*b2*cosf(2.0f*frequency);
    auto denominator = 1.0f + a1*a1 + a2*a2 + 2.0f*(a1 + a1*a2)*cosf(frequency) + 2.0f*a2*cosf(2.0f*frequency);
    return sqrtf(numerator / denominator);
}

// For details, see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
IIR2 IIR2::lowPass(float cutoffFrequency, int samplingRate)
{
    const auto Q = 0.707f;
    return lowPass(cutoffFrequency, Q, samplingRate);
}

IIR2 IIR2::lowPass(float cutoffFrequency,
                 float Q,
                 int samplingRate)
{
    IIR2 result;

    const double w0 = 2.0 * Math::kPiD * cutoffFrequency / samplingRate;
    const double cw0 = cos(w0);
    const double sw0 = sin(w0);
    const double alpha = sw0 / (2.0 * Q);

    double a0 = 1.0 + alpha;
    result.a1 = -2.0 * cw0;
    result.a2 = 1.0 - alpha;

    result.b0 = (1.0 - cw0) / 2.0;
    result.b1 = 1.0 - cw0;
    result.b2 = (1.0 - cw0) / 2.0;

    result.a1 /= a0;
    result.a2 /= a0;
    result.b0 /= a0;
    result.b1 /= a0;
    result.b2 /= a0;

    return result;
}

// For details, see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
IIR2 IIR2::highPass(float cutoffFrequency, int samplingRate)
{
    const auto Q = 0.707f;
    return highPass(cutoffFrequency, Q, samplingRate);
}

IIR2 IIR2::highPass(float cutoffFrequency,
                  float Q,
                  int samplingRate)
{
    IIR2 result;

    const auto w0 = 2.0f * Math::kPi * cutoffFrequency / samplingRate;
    const auto cw0 = cosf(w0);
    const auto sw0 = sinf(w0);
    const auto alpha = sw0 / (2.0f * Q);

    auto a0 = 1.0f + alpha;
    result.a1 = -2.0f * cw0;
    result.a2 = 1.0f - alpha;

    result.b0 = (1.0f + cw0) / 2.0f;
    result.b1 = -(1.0f + cw0);
    result.b2 = (1.0f + cw0) / 2.0f;

    result.a1 /= a0;
    result.a2 /= a0;
    result.b0 /= a0;
    result.b1 /= a0;
    result.b2 /= a0;

    return result;
}

// For details, see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
IIR2 IIR2::bandPass(float lowCutoffFrequency,
                  float highCutoffFrequency,
                  int samplingRate)
{
    IIR2 result;

    const auto cutoffFrequency = sqrtf(lowCutoffFrequency * highCutoffFrequency);
    const auto qInverse = (highCutoffFrequency - lowCutoffFrequency) / cutoffFrequency;
    const auto w0 = 2.0f * Math::kPi * cutoffFrequency / samplingRate;
    const auto cw0 = cosf(w0);
    const auto sw0 = sinf(w0);
    const auto alpha = sw0 * qInverse / 2.0f;

    auto a0 = 1.0f + alpha;
    result.a1 = -2.0f * cw0;
    result.a2 = 1.0f - alpha;

    result.b0 = alpha;
    result.b1 = 0.0f;
    result.b2 = -alpha;

    result.a1 /= a0;
    result.a2 /= a0;
    result.b0 /= a0;
    result.b1 /= a0;
    result.b2 /= a0;

    return result;
}

// For details, see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
IIR2 IIR2::lowShelf(float cutoffFrequency, float gain, int samplingRate)
{
    const auto Q = 0.707f;
    return lowShelf(cutoffFrequency, Q, gain, samplingRate);
}

IIR2 IIR2::lowShelf(float cutoffFrequency,
                  float Q,
                  float gain,
                  int samplingRate)
{
    IIR2 result;

    const auto w0 = 2.0f * Math::kPi * cutoffFrequency / samplingRate;
    const auto cw0 = cosf(w0);
    const auto sw0 = sinf(w0);
    const auto alpha = sw0 / (2.0f * Q);
    const auto A = sqrtf(gain);

    auto a0 = (A + 1.0f) + (A - 1.0f) * cw0 + 2.0f * sqrtf(A) * alpha;
    result.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw0);
    result.a2 = (A + 1.0f) + (A - 1.0f) * cw0 - 2.0f * sqrtf(A) * alpha;

    result.b0 = A * ((A + 1.0f) - (A - 1.0f) * cw0 + 2.0f * sqrtf(A) * alpha);
    result.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw0);
    result.b2 = A * ((A + 1.0f) - (A - 1.0f) * cw0 - 2.0f * sqrtf(A) * alpha);

    result.a1 /= a0;
    result.a2 /= a0;
    result.b0 /= a0;
    result.b1 /= a0;
    result.b2 /= a0;

    return result;
}

// For details, see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
IIR2 IIR2::highShelf(float cutoffFrequency, float gain, int samplingRate)
{
    const auto Q = 0.707f;
    return highShelf(cutoffFrequency, Q, gain, samplingRate);
}

IIR2 IIR2::highShelf(float cutoffFrequency,
                   float Q,
                   float gain,
                   int samplingRate)
{
    IIR2 result;

    const auto w0 = 2.0 * Math::kPiD * cutoffFrequency / samplingRate;
    const auto cw0 = cos(w0);
    const auto sw0 = sin(w0);
    const auto alpha = sw0 / (2.0 * Q);
    const auto A = sqrt(gain);

    auto a0 = (A + 1.0) - (A - 1.0) * cw0 + 2.0 * sqrt(A) * alpha;
    result.a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw0);
    result.a2 = (A + 1.0) - (A - 1.0) * cw0 - 2.0 * sqrt(A) * alpha;

    result.b0 = A * ((A + 1.0) + (A - 1.0) * cw0 + 2.0 * sqrt(A) * alpha);
    result.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw0);
    result.b2 = A * ((A + 1.0) + (A - 1.0) * cw0 - 2.0 * sqrt(A) * alpha);

    result.a1 /= a0;
    result.a2 /= a0;
    result.b0 /= a0;
    result.b1 /= a0;
    result.b2 /= a0;

    return result;
}

// For details, see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
// For bandwidth calculation, see http://www.sengpielaudio.com/calculator-bandwidth.htm
IIR2 IIR2::peaking(float lowCutoffFrequency,
                 float highCutoffFrequency,
                 float gain,
                 int samplingRate)
{
    IIR2 result;

    const auto cutoffFrequency = sqrt(lowCutoffFrequency * highCutoffFrequency);
    const auto qInverse = (highCutoffFrequency - lowCutoffFrequency) / cutoffFrequency;
    const auto w0 = 2.0f * Math::kPi * cutoffFrequency / samplingRate;
    const auto cw0 = cosf(w0);
    const auto sw0 = sinf(w0);
    const auto alpha = sw0 * qInverse / 2.0f;
    const auto A = sqrtf(gain);

    auto a0 = 1.0f + alpha / A;
    result.a1 = -2.0f * cw0;
    result.a2 = 1.0f - alpha / A;

    result.b0 = 1.0f + alpha * A;
    result.b1 = -2.0f * cw0;
    result.b2 = 1.0f - alpha * A;

    result.a1 /= a0;
    result.a2 /= a0;
    result.b0 /= a0;
    result.b1 /= a0;
    result.b2 /= a0;

    return result;
}

void IIR2::bandFilters(IIR2* filters, int samplingRate)
{
    filters[0] = lowPass(Bands::kHighCutoffFrequencies[0], samplingRate);
    filters[1] = bandPass(Bands::kLowCutoffFrequencies[1], Bands::kHighCutoffFrequencies[1], samplingRate);
    filters[2] = highPass(Bands::kLowCutoffFrequencies[2], samplingRate);
}


// --------------------------------------------------------------------------------------------------------------------
// IIRFilterer2
// --------------------------------------------------------------------------------------------------------------------

IIR2Filterer::IIR2Filterer()
{
#if defined(IPL_ENABLE_FLOAT8)
    mResetFilterDispatch = (gSIMDLevel() >= SIMDLevel::AVX) ? &IIR2Filterer::resetFilter_float8 : &IIR2Filterer::resetFilter_float4;
    mSetFilterDispatch = (gSIMDLevel() >= SIMDLevel::AVX) ? &IIR2Filterer::setFilter_float8 : &IIR2Filterer::setFilter_float4;
    mApplyDispatch = (gSIMDLevel() >= SIMDLevel::AVX) ? &IIR2Filterer::apply_float8 : &IIR2Filterer::apply_float4;
#else
    mResetFilterDispatch = &IIR2Filterer::resetFilter_float4;
    mSetFilterDispatch = &IIR2Filterer::setFilter_float4;
    mApplyDispatch = &IIR2Filterer::apply_float4;
#endif

    resetFilter();
    reset();
}

void IIR2Filterer::reset()
{
    mXm1 = 0.0f;
    mXm2 = 0.0f;
    mYm1 = 0.0f;
    mYm2 = 0.0f;
}

void IIR2Filterer::copyState(const IIR2Filterer& other)
{
    mXm1 = other.mXm1;
    mXm2 = other.mXm2;
    mYm1 = other.mYm1;
    mYm2 = other.mYm2;
}

void IIR2Filterer::resetFilter()
{
    (this->*mResetFilterDispatch)();
}

void IIR2Filterer::resetFilter_float4()
{
    memset(mCoeffs4, 0, 4 * 8 * sizeof(float));
}

void IIR2Filterer::setFilter(const IIR2& filter)
{
    (this->*mSetFilterDispatch)(filter);
}

void IIR2Filterer::setFilter_float4(const IIR2& filter)
{
    mFilter = filter;

    mCoeffs4[0][0] = 0.0f;
    mCoeffs4[1][0] = 0.0f;
    mCoeffs4[2][0] = 0.0f;
    mCoeffs4[3][0] = filter.b0;
    mCoeffs4[4][0] = filter.b1;
    mCoeffs4[5][0] = filter.b2;
    mCoeffs4[6][0] = -filter.a1;
    mCoeffs4[7][0] = -filter.a2;

    mCoeffs4[0][1] = 0.0f;
    mCoeffs4[1][1] = 0.0f;
    mCoeffs4[2][1] = filter.b0;
    mCoeffs4[3][1] = filter.b1;
    mCoeffs4[4][1] = filter.b2;
    mCoeffs4[5][1] = 0.0f;
    mCoeffs4[6][1] = -filter.a2;
    mCoeffs4[7][1] = 0.0f;

    mCoeffs4[0][2] = 0.0f;
    mCoeffs4[1][2] = filter.b0;
    mCoeffs4[2][2] = filter.b1;
    mCoeffs4[3][2] = filter.b2;
    mCoeffs4[4][2] = 0.0f;
    mCoeffs4[5][2] = 0.0f;
    mCoeffs4[6][2] = 0.0f;
    mCoeffs4[7][2] = 0.0f;

    mCoeffs4[0][3] = filter.b0;
    mCoeffs4[1][3] = filter.b1;
    mCoeffs4[2][3] = filter.b2;
    mCoeffs4[3][3] = 0.0f;
    mCoeffs4[4][3] = 0.0f;
    mCoeffs4[5][3] = 0.0f;
    mCoeffs4[6][3] = 0.0f;
    mCoeffs4[7][3] = 0.0f;

    for (auto i = 0; i < 8; ++i)
    {
        mCoeffs4[i][1] += -filter.a1 * mCoeffs4[i][0];
        mCoeffs4[i][2] += -filter.a1 * mCoeffs4[i][1] + -filter.a2 * mCoeffs4[i][0];
        mCoeffs4[i][3] += -filter.a1 * mCoeffs4[i][2] + -filter.a2 * mCoeffs4[i][1];
    }
}

float IIR2Filterer::apply(float in)
{
    auto x = in + 1e-9f;
    auto y = mFilter.b0 * x + mFilter.b1 * mXm1 + mFilter.b2 * mXm2 - mFilter.a1 * mYm1 - mFilter.a2 * mYm2;
    mXm2 = mXm1;
    mXm1 = x;
    mYm2 = mYm1;
    mYm1 = y;

    return y;
}

float4_t IIR2Filterer::apply(float4_t in)
{
    auto coeffxp3 = float4::load(&mCoeffs4[0][0]);
    auto coeffxp2 = float4::load(&mCoeffs4[1][0]);
    auto coeffxp1 = float4::load(&mCoeffs4[2][0]);
    auto coeffx   = float4::load(&mCoeffs4[3][0]);
    auto coeffxm1 = float4::load(&mCoeffs4[4][0]);
    auto coeffxm2 = float4::load(&mCoeffs4[5][0]);
    auto coeffym1 = float4::load(&mCoeffs4[6][0]);
    auto coeffym2 = float4::load(&mCoeffs4[7][0]);

    auto xm2 = float4::set1(mXm2);
    auto xm1 = float4::set1(mXm1);
    auto ym2 = float4::set1(mYm2);
    auto ym1 = float4::set1(mYm1);

    in = float4::add(in, float4::set1(1e-9f));

    auto x   = float4::replicate<0>(in);
    auto xp1 = float4::replicate<1>(in);
    auto xp2 = float4::replicate<2>(in);
    auto xp3 = float4::replicate<3>(in);

    auto y = float4::mul(coeffxp3, xp3);
    y = float4::add(y, float4::mul(coeffxp2, xp2));
    y = float4::add(y, float4::mul(coeffxp1, xp1));
    y = float4::add(y, float4::mul(coeffx, x));
    y = float4::add(y, float4::mul(coeffxm1, xm1));
    y = float4::add(y, float4::mul(coeffxm2, xm2));
    y = float4::add(y, float4::mul(coeffym1, ym1));
    y = float4::add(y, float4::mul(coeffym2, ym2));

    mXm1 = float4::get1(xp3);
    mXm2 = float4::get1(xp2);

    mYm1 = float4::get1(float4::replicate<3>(y));
    mYm2 = float4::get1(float4::replicate<2>(y));

    return y;
}

void IIR2Filterer::apply(int size,
                        const float* in,
                        float* out)
{
    (this->*mApplyDispatch)(size, in, out);
}

void IIR2Filterer::apply_float4(int size,
                               const float* in,
                               float* out)
{
    auto coeffxp3 = float4::load(&mCoeffs4[0][0]);
    auto coeffxp2 = float4::load(&mCoeffs4[1][0]);
    auto coeffxp1 = float4::load(&mCoeffs4[2][0]);
    auto coeffx   = float4::load(&mCoeffs4[3][0]);
    auto coeffxm1 = float4::load(&mCoeffs4[4][0]);
    auto coeffxm2 = float4::load(&mCoeffs4[5][0]);
    auto coeffym1 = float4::load(&mCoeffs4[6][0]);
    auto coeffym2 = float4::load(&mCoeffs4[7][0]);

    auto xm2 = float4::set1(mXm2);
    auto xm1 = float4::set1(mXm1);
    auto ym2 = float4::set1(mYm2);
    auto ym1 = float4::set1(mYm1);

    auto simdSize = size & ~3;

    if (simdSize > 0)
    {
        mXm1 = in[simdSize - 1];
        mXm2 = in[simdSize - 2];
    }

    auto epsilon = float4::set1(1e-9f);

    for (auto i = 0; i < simdSize; i += 4)
    {
        auto in4 = float4::loadu(&in[i]);
        in4 = float4::add(in4, epsilon);

        auto x = float4::replicate<0>(in4);
        auto xp1 = float4::replicate<1>(in4);
        auto xp2 = float4::replicate<2>(in4);
        auto xp3 = float4::replicate<3>(in4);

        auto y = float4::mul(coeffxp3, xp3);
        y = float4::add(y, float4::mul(coeffxp2, xp2));
        y = float4::add(y, float4::mul(coeffxp1, xp1));
        y = float4::add(y, float4::mul(coeffx, x));
        y = float4::add(y, float4::mul(coeffxm1, xm1));
        y = float4::add(y, float4::mul(coeffxm2, xm2));
        y = float4::add(y, float4::mul(coeffym1, ym1));
        y = float4::add(y, float4::mul(coeffym2, ym2));

        xm2 = xp2;
        xm1 = xp3;
        ym2 = float4::replicate<2>(y);
        ym1 = float4::replicate<3>(y);

        float4::storeu(&out[i], y);
    }

    if (simdSize > 0)
    {
        mYm1 = out[simdSize - 1];
        mYm2 = out[simdSize - 2];
    }

    for (auto i = simdSize; i < size; ++i)
    {
        out[i] = apply(in[i]);
    }
}


// --------------------------------------------------------------------------------------------------------------------
// IIR8
// --------------------------------------------------------------------------------------------------------------------

float IIR8::spectrum(float frequency)
{
    double result = 1.0;

    for (auto i = 0; i < 4; ++i)
    {
        double b0 = m_iir[i].b0;
        double b1 = m_iir[i].b1;
        double b2 = m_iir[i].b2;
        double a1 = m_iir[i].a1;
        double a2 = m_iir[i].a2;

        double numerator = b0 * b0 + b1 * b1 + b2 * b2 + 2.0 * (b0 * b1 + b1 * b2) * cos(frequency) + 2.0 * b0 * b2 * cos(2.0 * frequency);
        double denominator = 1.0 + a1 * a1 + a2 * a2 + 2.0 * (a1 + a1 * a2) * cos(frequency) + 2.0 * a2 * cos(2.0 * frequency);

        result *= sqrt(numerator / denominator);
    }

    return (float) result;
}

float IIR8::spectrumPeak()
{
    const auto kNumSpectrumSamples = 10000;

    auto peak = 0.0f;

    for (auto i = 0; i < kNumSpectrumSamples; ++i)
    {
        auto value = spectrum((i * Math::kPi) / kNumSpectrumSamples);
        if (Math::isFinite(value))
        {
            peak = std::max(value, peak);
        }
    }

    return peak;
}

IIR8 IIR8::lowPass(float cutoff, int samplingRate)
{
    const float q[] = {0.50979558f, 0.60134489f, 0.89997622f, 2.5629154f};
    auto result = IIR8{};
    for (auto i = 0; i < 4; ++i)
    {
        result.m_iir[i] = IIR2::lowPass(cutoff, q[i], samplingRate);
    }
    return result;
}

IIR8 IIR8::highPass(float cutoff, int samplingRate)
{
    const float q[] = {0.50979558f, 0.60134489f, 0.89997622f, 2.5629154f};
    auto result = IIR8{};
    for (auto i = 0; i < 4; ++i)
    {
        result.m_iir[i] = IIR2::highPass(cutoff, q[i], samplingRate);
    }
    return result;
}

IIR8 IIR8::bandPass(float lowCutoff, float highCutoff, int samplingRate)
{
    const float q[] = {0.54119610f, 1.3065630f};
    auto result = IIR8{};
    for (auto i = 0; i < 2; ++i)
    {
        result.m_iir[i] = IIR2::lowPass(highCutoff, q[i], samplingRate);
        result.m_iir[i + 2] = IIR2::highPass(lowCutoff, q[i], samplingRate);
    }
    return result;
}

IIR8 IIR8::lowShelf(float cutoff, float gain, int samplingRate)
{
    const float q[] = {0.50979558f, 0.60134489f, 0.89997622f, 2.5629154f};
    auto result = IIR8{};
    for (auto i = 0; i < 4; ++i)
    {
        result.m_iir[i] = IIR2::lowShelf(cutoff, q[i], powf(gain, 0.25f), samplingRate);
    }
    return result;
}

IIR8 IIR8::highShelf(float cutoff, float gain, int samplingRate)
{
    const float q[] = {0.50979558f, 0.60134489f, 0.89997622f, 2.5629154f};
    auto result = IIR8{};
    for (auto i = 0; i < 4; ++i)
    {
        result.m_iir[i] = IIR2::highShelf(cutoff, q[i], powf(gain, 0.25f), samplingRate);
    }
    return result;
}

float bw_to_q(float fl, float fh)
{
    auto fc = sqrtf(fl * fh);
    auto bw = fh - fl;
    auto q = fc / bw;
    return q;
}

IIR2 peaking_q(float lowCutoff, float highCutoff, float q, float gain, int samplingRate)
{
    IIR2 result{};

    const auto cutoffFrequency = sqrt(lowCutoff * highCutoff);

    const auto qInverse = 1.0f / q;

    const auto w0 = 2.0f * Math::kPi * cutoffFrequency / samplingRate;
    const auto cw0 = cosf(w0);
    const auto sw0 = sinf(w0);
    const auto alpha = sw0 * qInverse / 2.0f;
    const auto A = sqrtf(gain);

    auto a0 = 1.0f + alpha / A;
    result.a1 = -2.0f * cw0;
    result.a2 = 1.0f - alpha / A;

    result.b0 = 1.0f + alpha * A;
    result.b1 = -2.0f * cw0;
    result.b2 = 1.0f - alpha * A;

    result.a1 /= a0;
    result.a2 /= a0;
    result.b0 /= a0;
    result.b1 /= a0;
    result.b2 /= a0;

    return result;
}

IIR8 IIR8::peaking(float lowCutoff, float highCutoff, float gain, int samplingRate)
{
    const float q[] = {0.54119610f, 1.3065630f};

    auto result = IIR8{};

    for (auto i = 0; i < 2; ++i)
    {
        result.m_iir[i] = IIR2::lowShelf(highCutoff, q[i], powf(gain, 0.5f), samplingRate);
        result.m_iir[i + 2] = IIR2::highShelf(lowCutoff, q[i], powf(gain, 0.5f), samplingRate);
    }

    for (auto i = 0; i < 4; ++i)
    {
        result.m_iir[i].b0 /= powf(gain, 0.25f);
        result.m_iir[i].b1 /= powf(gain, 0.25f);
        result.m_iir[i].b2 /= powf(gain, 0.25f);
    }

    return result;
}

void IIR8::bandFilters(IIR8* filters, int samplingRate)
{
    filters[0] = lowPass(Bands::kHighCutoffFrequencies[0], samplingRate);
 
    for (auto i = 1; i < Bands::kNumBands - 1; ++i)
    {
        filters[i] = bandPass(Bands::kLowCutoffFrequencies[i], Bands::kHighCutoffFrequencies[i], samplingRate);

        for (auto j = 0; j < 4; ++j)
        {
            filters[i].m_iir[j].b0 *= Bands::kBandPassNorm[i];
            filters[i].m_iir[j].b1 *= Bands::kBandPassNorm[i];
            filters[i].m_iir[j].b2 *= Bands::kBandPassNorm[i];
        }
    }
    
    filters[Bands::kNumBands - 1] = highPass(Bands::kLowCutoffFrequencies[Bands::kNumBands - 1], samplingRate);
}


// --------------------------------------------------------------------------------------------------------------------
// IIR8Filterer
// --------------------------------------------------------------------------------------------------------------------

void IIR8Filterer::reset()
{
    for (auto i = 0; i < 4; ++i)
    {
        m_filterers[i].reset();
    }
}

void IIR8Filterer::copyState(const IIR8Filterer& other)
{
    for (auto i = 0; i < 4; ++i)
    {
        m_filterers[i].copyState(other.m_filterers[i]);
    }
}

void IIR8Filterer::resetFilter()
{
    for (auto i = 0; i < 4; ++i)
    {
        m_filterers[i].resetFilter();
    }
}

void IIR8Filterer::setFilter(const IIR8& filter)
{
    for (auto i = 0; i < 4; ++i)
    {
        m_filterers[i].setFilter(filter.m_iir[i]);
    }
}

float IIR8Filterer::apply(float in)
{
    auto out = in;
    for (auto i = 0; i < 4; ++i)
    {
        out = m_filterers[i].apply(out);
    }
    return out;
}

float4_t IIR8Filterer::apply(float4_t in)
{
    auto out = in;
    for (auto i = 0; i < 4; ++i)
    {
        out = m_filterers[i].apply(out);
    }
    return out;
}

void IIR8Filterer::apply(int size, const float* in, float* out)
{
    m_filterers[0].apply(size, in, out);
    for (auto i = 1; i < 4; ++i)
    {
        m_filterers[i].apply(size, out, out);
    }
}


// --------------------------------------------------------------------------------------------------------------------
// IIR
// --------------------------------------------------------------------------------------------------------------------

bool IIR::sUseOrder8 = false;

float IIR::spectrum(float frequency)
{
    if (sUseOrder8)
        return m_iir8.spectrum(frequency);
    else
        return m_iir2.spectrum(frequency);
}

IIR IIR::lowPass(float cutoff, int samplingRate)
{
    IIR result{};
    result.m_iir2 = IIR2::lowPass(cutoff, samplingRate);
    result.m_iir8 = IIR8::lowPass(cutoff, samplingRate);
    return result;
}

IIR IIR::highPass(float cutoff, int samplingRate)
{
    IIR result{};
    result.m_iir2 = IIR2::highPass(cutoff, samplingRate);
    result.m_iir8 = IIR8::highPass(cutoff, samplingRate);
    return result;
}

IIR IIR::bandPass(float lowCutoff, float highCutoff, int samplingRate)
{
    IIR result{};
    result.m_iir2 = IIR2::bandPass(lowCutoff, highCutoff, samplingRate);
    result.m_iir8 = IIR8::bandPass(lowCutoff, highCutoff, samplingRate);
    return result;
}

IIR IIR::lowShelf(float cutoff, float gain, int samplingRate)
{
    IIR result{};
    result.m_iir2 = IIR2::lowShelf(cutoff, gain, samplingRate);
    result.m_iir8 = IIR8::lowShelf(cutoff, gain, samplingRate);
    return result;
}

IIR IIR::highShelf(float cutoff, float gain, int samplingRate)
{
    IIR result{};
    result.m_iir2 = IIR2::highShelf(cutoff, gain, samplingRate);
    result.m_iir8 = IIR8::highShelf(cutoff, gain, samplingRate);
    return result;
}

IIR IIR::peaking(float lowCutoff, float highCutoff, float gain, int samplingRate)
{
    IIR result{};
    result.m_iir2 = IIR2::peaking(lowCutoff, highCutoff, gain, samplingRate);
    result.m_iir8 = IIR8::peaking(lowCutoff, highCutoff, gain, samplingRate);
    return result;
}

void IIR::bandFilters(IIR* filters, int samplingRate)
{
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        if (i == 0)
            filters[i] = lowPass(Bands::kHighCutoffFrequencies[i], samplingRate);
        else if (i == Bands::kNumBands - 1)
            filters[i] = highPass(Bands::kLowCutoffFrequencies[i], samplingRate);
        else
            filters[i] = bandPass(Bands::kLowCutoffFrequencies[i], Bands::kHighCutoffFrequencies[i], samplingRate);
    }
}


// --------------------------------------------------------------------------------------------------------------------
// IIRFilterer
// --------------------------------------------------------------------------------------------------------------------

bool IIRFilterer::sEnableSwitching = false;

IIRFilterer::IIRFilterer()
{
    if (sEnableSwitching)
    {
        m_reset = &IIRFilterer::reset_switchable;
        m_copyState = &IIRFilterer::copyState_switchable;
        m_resetFilter = &IIRFilterer::resetFilter_switchable;
        m_setFilter = &IIRFilterer::setFilter_switchable;
        m_applyFloat = &IIRFilterer::applyFloat_switchable;
        m_applyFloat4 = &IIRFilterer::applyFloat4_switchable;
#if defined(IPL_ENABLE_FLOAT8)
        m_applyFloat8 = &IIRFilterer::applyFloat8_switchable;
#endif
        m_applyFloatArray = &IIRFilterer::applyFloatArray_switchable;
    }
    else
    {
        m_reset = &IIRFilterer::reset_iir2;
        m_copyState = &IIRFilterer::copyState_iir2;
        m_resetFilter = &IIRFilterer::resetFilter_iir2;
        m_setFilter = &IIRFilterer::setFilter_iir2;
        m_applyFloat = &IIRFilterer::applyFloat_iir2;
        m_applyFloat4 = &IIRFilterer::applyFloat4_iir2;
#if defined(IPL_ENABLE_FLOAT8)
        m_applyFloat8 = &IIRFilterer::applyFloat8_iir2;
#endif
        m_applyFloatArray = &IIRFilterer::applyFloatArray_iir2;
    }
}

void IIRFilterer::reset()
{
    (this->*m_reset)();
}

void IIRFilterer::copyState(const IIRFilterer& other)
{
    (this->*m_copyState)(other);
}

void IIRFilterer::resetFilter()
{
    (this->*m_resetFilter)();
}

void IIRFilterer::setFilter(const IIR& filter)
{
    (this->*m_setFilter)(filter);
}

float IIRFilterer::apply(float in)
{
    return (this->*m_applyFloat)(in);
}

float4_t IIRFilterer::apply(float4_t in)
{
    return (this->*m_applyFloat4)(in);
}

void IIRFilterer::apply(int size, const float* in, float* out)
{
    (this->*m_applyFloatArray)(size, in, out);
}

void IIRFilterer::reset_iir2()
{
    m_filterer2.reset();
}

void IIRFilterer::copyState_iir2(const IIRFilterer& other)
{
    m_filterer2.copyState(other.m_filterer2);
}

void IIRFilterer::resetFilter_iir2()
{
    m_filterer2.resetFilter();
}

void IIRFilterer::setFilter_iir2(const IIR& filter)
{
    m_filterer2.setFilter(filter.m_iir2);
}

float IIRFilterer::applyFloat_iir2(float in)
{
    return m_filterer2.apply(in);
}

float4_t IIRFilterer::applyFloat4_iir2(float4_t in)
{
    return m_filterer2.apply(in);
}

void IIRFilterer::applyFloatArray_iir2(int size, const float* in, float* out)
{
    m_filterer2.apply(size, in, out);
}

void IIRFilterer::reset_switchable()
{
    m_filterer2.reset();
    m_filterer8.reset();
}

void IIRFilterer::copyState_switchable(const IIRFilterer& other)
{
    m_filterer2.copyState(other.m_filterer2);
    m_filterer8.copyState(other.m_filterer8);
}

void IIRFilterer::resetFilter_switchable()
{
    m_filterer2.resetFilter();
    m_filterer8.resetFilter();
}

void IIRFilterer::setFilter_switchable(const IIR& filter)
{
    m_filterer2.setFilter(filter.m_iir2);
    m_filterer8.setFilter(filter.m_iir8);
}

float IIRFilterer::applyFloat_switchable(float in)
{
    if (IIR::sUseOrder8)
    {
        m_filterer2.reset();
        return m_filterer8.apply(in);
    }
    else
    {
        m_filterer8.reset();
        return m_filterer2.apply(in);
    }
}

float4_t IIRFilterer::applyFloat4_switchable(float4_t in)
{
    if (IIR::sUseOrder8)
    {
        m_filterer2.reset();
        return m_filterer8.apply(in);
    }
    else
    {
        m_filterer8.reset();
        return m_filterer2.apply(in);
    }
}

void IIRFilterer::applyFloatArray_switchable(int size, const float* in, float* out)
{
    if (IIR::sUseOrder8)
    {
        m_filterer2.reset();
        m_filterer8.apply(size, in, out);
    }
    else
    {
        m_filterer8.reset();
        m_filterer2.apply(size, in, out);
    }
}

}
