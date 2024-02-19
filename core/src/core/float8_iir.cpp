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

#if defined(IPL_ENABLE_FLOAT8)

#include "iir.h"

#include "context.h"
#include "float8.h"
#include "math_functions.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IIRFilterer
// --------------------------------------------------------------------------------------------------------------------

void IIRFilterer::resetFilter_float8()
{
    memset(mCoeffs8, 0, 8 * 12 * sizeof(float));
}

void IIRFilterer::setFilter_float8(const IIR& filter)
{
    mFilter = filter;

    memset(mCoeffs8, 0, 12 * 8 * sizeof(float));

    mCoeffs8[7][0] = filter.b0;
    mCoeffs8[8][0] = filter.b1;
    mCoeffs8[9][0] = filter.b2;
    mCoeffs8[10][0] = -filter.a1;
    mCoeffs8[11][0] = -filter.a2;

    mCoeffs8[6][1] = filter.b0;
    mCoeffs8[7][1] = filter.b1;
    mCoeffs8[8][1] = filter.b2;
    mCoeffs8[10][1] = -filter.a2;

    mCoeffs8[5][2] = filter.b0;
    mCoeffs8[6][2] = filter.b1;
    mCoeffs8[7][2] = filter.b2;

    mCoeffs8[4][3] = filter.b0;
    mCoeffs8[5][3] = filter.b1;
    mCoeffs8[6][3] = filter.b2;

    mCoeffs8[3][4] = filter.b0;
    mCoeffs8[4][4] = filter.b1;
    mCoeffs8[5][4] = filter.b2;

    mCoeffs8[2][5] = filter.b0;
    mCoeffs8[3][5] = filter.b1;
    mCoeffs8[4][5] = filter.b2;

    mCoeffs8[1][6] = filter.b0;
    mCoeffs8[2][6] = filter.b1;
    mCoeffs8[3][6] = filter.b2;

    mCoeffs8[0][7] = filter.b0;
    mCoeffs8[1][7] = filter.b1;
    mCoeffs8[2][7] = filter.b2;

    for (auto i = 0; i < 12; ++i)
    {
        mCoeffs8[i][1] += -filter.a1 * mCoeffs8[i][0];
        mCoeffs8[i][2] += -filter.a1 * mCoeffs8[i][1] + -filter.a2 * mCoeffs8[i][0];
        mCoeffs8[i][3] += -filter.a1 * mCoeffs8[i][2] + -filter.a2 * mCoeffs8[i][1];
        mCoeffs8[i][4] += -filter.a1 * mCoeffs8[i][3] + -filter.a2 * mCoeffs8[i][2];
        mCoeffs8[i][5] += -filter.a1 * mCoeffs8[i][4] + -filter.a2 * mCoeffs8[i][3];
        mCoeffs8[i][6] += -filter.a1 * mCoeffs8[i][5] + -filter.a2 * mCoeffs8[i][4];
        mCoeffs8[i][7] += -filter.a1 * mCoeffs8[i][6] + -filter.a2 * mCoeffs8[i][5];
    }
}

float8_t IIRFilterer::apply(float8_t in)
{
    auto coeffxp7 = float8::load(&mCoeffs8[0][0]);
    auto coeffxp6 = float8::load(&mCoeffs8[1][0]);
    auto coeffxp5 = float8::load(&mCoeffs8[2][0]);
    auto coeffxp4 = float8::load(&mCoeffs8[3][0]);
    auto coeffxp3 = float8::load(&mCoeffs8[4][0]);
    auto coeffxp2 = float8::load(&mCoeffs8[5][0]);
    auto coeffxp1 = float8::load(&mCoeffs8[6][0]);
    auto coeffx   = float8::load(&mCoeffs8[7][0]);
    auto coeffxm1 = float8::load(&mCoeffs8[8][0]);
    auto coeffxm2 = float8::load(&mCoeffs8[9][0]);
    auto coeffym1 = float8::load(&mCoeffs8[10][0]);
    auto coeffym2 = float8::load(&mCoeffs8[11][0]);

    auto xm1 = float8::set1(&mXm1);
    auto xm2 = float8::set1(&mXm2);
    auto ym1 = float8::set1(&mYm1);
    auto ym2 = float8::set1(&mYm2);

    in = float8::add(in, float8::set1(1e-9f));

    auto s0 = float8::replicateHalves<0>(in);
    auto s1 = float8::replicateHalves<1>(in);
    auto s2 = float8::replicateHalves<2>(in);
    auto s3 = float8::replicateHalves<3>(in);

    auto x   = float8::replicateLower(s0);
    auto xp1 = float8::replicateLower(s1);
    auto xp2 = float8::replicateLower(s2);
    auto xp3 = float8::replicateLower(s3);
    auto xp4 = float8::replicateUpper(s0);
    auto xp5 = float8::replicateUpper(s1);
    auto xp6 = float8::replicateUpper(s2);
    auto xp7 = float8::replicateUpper(s3);

    auto y = float8::mul(coeffxp7, xp7);
    y = float8::add(y, float8::mul(coeffxp6, xp6));
    y = float8::add(y, float8::mul(coeffxp5, xp5));
    y = float8::add(y, float8::mul(coeffxp4, xp4));
    y = float8::add(y, float8::mul(coeffxp3, xp3));
    y = float8::add(y, float8::mul(coeffxp2, xp2));
    y = float8::add(y, float8::mul(coeffxp1, xp1));
    y = float8::add(y, float8::mul(coeffx, x));
    y = float8::add(y, float8::mul(coeffxm1, xm1));
    y = float8::add(y, float8::mul(coeffxm2, xm2));
    y = float8::add(y, float8::mul(coeffym1, ym1));
    y = float8::add(y, float8::mul(coeffym2, ym2));

    mXm1 = float8::get1(xp7);
    mXm2 = float8::get1(xp6);

    s2 = float8::replicateHalves<2>(y);
    s3 = float8::replicateHalves<3>(y);

    ym2 = float8::replicateUpper(s2);
    ym1 = float8::replicateUpper(s3);

    mYm1 = float8::get1(ym1);
    mYm2 = float8::get1(ym2);

    return y;
}

void IIRFilterer::apply_float8(int size,
                               const float* in,
                               float* out)
{
    auto coeffxp7 = float8::load(&mCoeffs8[0][0]);
    auto coeffxp6 = float8::load(&mCoeffs8[1][0]);
    auto coeffxp5 = float8::load(&mCoeffs8[2][0]);
    auto coeffxp4 = float8::load(&mCoeffs8[3][0]);
    auto coeffxp3 = float8::load(&mCoeffs8[4][0]);
    auto coeffxp2 = float8::load(&mCoeffs8[5][0]);
    auto coeffxp1 = float8::load(&mCoeffs8[6][0]);
    auto coeffx   = float8::load(&mCoeffs8[7][0]);
    auto coeffxm1 = float8::load(&mCoeffs8[8][0]);
    auto coeffxm2 = float8::load(&mCoeffs8[9][0]);
    auto coeffym1 = float8::load(&mCoeffs8[10][0]);
    auto coeffym2 = float8::load(&mCoeffs8[11][0]);

    auto xm1 = float8::set1(&mXm1);
    auto xm2 = float8::set1(&mXm2);
    auto ym1 = float8::set1(&mYm1);
    auto ym2 = float8::set1(&mYm2);

    auto simdSize = size & ~7;

    if (simdSize > 0)
    {
        mXm1 = in[simdSize - 1];
        mXm2 = in[simdSize - 2];
    }

    auto epsilon = float8::set1(1e-9f);

    for (auto i = 0; i < simdSize; i += 8)
    {
        auto in8 = float8::loadu(&in[i]);
        in8 = float8::add(in8, epsilon);

        auto s0 = float8::replicateHalves<0>(in8);
        auto s1 = float8::replicateHalves<1>(in8);
        auto s2 = float8::replicateHalves<2>(in8);
        auto s3 = float8::replicateHalves<3>(in8);

        auto x   = float8::replicateLower(s0);
        auto xp1 = float8::replicateLower(s1);
        auto xp2 = float8::replicateLower(s2);
        auto xp3 = float8::replicateLower(s3);
        auto xp4 = float8::replicateUpper(s0);
        auto xp5 = float8::replicateUpper(s1);
        auto xp6 = float8::replicateUpper(s2);
        auto xp7 = float8::replicateUpper(s3);

        auto y = float8::mul(coeffxp7, xp7);
        y = float8::add(y, float8::mul(coeffxp6, xp6));
        y = float8::add(y, float8::mul(coeffxp5, xp5));
        y = float8::add(y, float8::mul(coeffxp4, xp4));
        y = float8::add(y, float8::mul(coeffxp3, xp3));
        y = float8::add(y, float8::mul(coeffxp2, xp2));
        y = float8::add(y, float8::mul(coeffxp1, xp1));
        y = float8::add(y, float8::mul(coeffx, x));
        y = float8::add(y, float8::mul(coeffxm1, xm1));
        y = float8::add(y, float8::mul(coeffxm2, xm2));
        y = float8::add(y, float8::mul(coeffym1, ym1));
        y = float8::add(y, float8::mul(coeffym2, ym2));

        float8::storeu(&out[i], y);

        xm1 = xp7;
        xm2 = xp6;

        s2 = float8::replicateHalves<2>(y);
        s3 = float8::replicateHalves<3>(y);

        ym2 = float8::replicateUpper(s2);
        ym1 = float8::replicateUpper(s3);
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

    float8::avoidTransitionPenalty();
}

}

#endif
