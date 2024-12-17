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

#include "array_math.h"
#include "float8.h"
#include "reverb_effect.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ReverbEffect
// --------------------------------------------------------------------------------------------------------------------

void IPL_FLOAT8_ATTR ReverbEffect::apply_float8(const float* reverbTimes,
                                const float* in,
                                float* out)
{
    PROFILE_FUNCTION();

    float clampedReverbTimes[Bands::kNumBands];
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        clampedReverbTimes[i] = std::max(0.1f, reverbTimes[i]);
    }

    memset(out, 0, mFrameSize * sizeof(float));

    float const lowCutoff[Bands::kNumBands] = {20.0f, 500.0f, 5000.0f};
    float const highCutoff[Bands::kNumBands] = {500.0f, 5000.0f, 22000.0f};

    for (auto i = 0; i < kNumDelays; ++i)
    {
        float absorptiveGains[Bands::kNumBands];
        calcAbsorptiveGains(clampedReverbTimes, mDelayValues[i], absorptiveGains);

        IIR iir[Bands::kNumBands];
        iir[0] = IIR::lowShelf(highCutoff[0], absorptiveGains[0], mSamplingRate);
        iir[1] = IIR::peaking(lowCutoff[1], highCutoff[1], absorptiveGains[1], mSamplingRate);
        iir[2] = IIR::highShelf(lowCutoff[2], absorptiveGains[2], mSamplingRate);

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mAbsorptive[i][j][mCurrent].setFilter(iir[j]);
        }
    }

    float toneCorrectionGains[Bands::kNumBands];
    calcToneCorrectionGains(clampedReverbTimes, toneCorrectionGains);

    IIR iir[Bands::kNumBands];
    iir[0] = IIR::lowShelf(highCutoff[0], toneCorrectionGains[0], mSamplingRate);
    iir[1] = IIR::peaking(lowCutoff[1], highCutoff[1], toneCorrectionGains[1], mSamplingRate);
    iir[2] = IIR::highShelf(lowCutoff[2], toneCorrectionGains[2], mSamplingRate);

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mToneCorrection[i][mCurrent].setFilter(iir[i]);
    }

    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].get(mFrameSize, mXOld[i]);

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mAbsorptive[i][j][mCurrent].apply(mFrameSize, mXOld[i], mXOld[i]);
        }
    }

    float8_t xOld[kNumDelays];
    float8_t xNew[kNumDelays];
    for (auto i = 0; i < mFrameSize; i += 8)
    {
        for (auto j = 0; j < kNumDelays; ++j)
        {
            xOld[j] = float8::loadu(&mXOld[j][i]);
        }

        multiplyHadamardMatrix(xOld, xNew);

        for (auto j = 0; j < kNumDelays; ++j)
        {
            float8::storeu(&mXNew[j][i], xNew[j]);
        }
    }

    for (auto i = 0; i < kNumDelays; ++i)
    {
        ArrayMath::add(mFrameSize, mXNew[i], in, mXNew[i]);

        mDelayLines[i].put(mFrameSize, mXNew[i]);
    }

    for (auto i = 1; i < kNumDelays; ++i)
    {
        ArrayMath::add(mFrameSize, mXOld[0], mXOld[i], mXOld[0]);
    }

    ArrayMath::scale(mFrameSize, mXOld[0], 1.0f / kNumDelays, mXOld[0]);

    for (auto i = 0; i < mFrameSize; i += 8)
    {
        xOld[0] = float8::loadu(&mXOld[0][i]);

        for (auto j = 0; j < kNumAllpasses; ++j)
        {
            xOld[0] = mAllpass[j].apply(xOld[0]);
        }

        float8::storeu(&out[i], xOld[0]);
    }

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mToneCorrection[i][mCurrent].apply(mFrameSize, out, out);
    }

    float8::avoidTransitionPenalty();
}

void IPL_FLOAT8_ATTR ReverbEffect::tail_float8(float* out)
{
    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].get(mFrameSize, mXOld[i]);

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mAbsorptive[i][j][mCurrent].apply(mFrameSize, mXOld[i], mXOld[i]);
        }
    }

    float8_t xOld[kNumDelays];
    float8_t xNew[kNumDelays];
    for (auto i = 0; i < mFrameSize; i += 8)
    {
        for (auto j = 0; j < kNumDelays; ++j)
        {
            xOld[j] = float8::loadu(&mXOld[j][i]);
        }

        multiplyHadamardMatrix(xOld, xNew);

        for (auto j = 0; j < kNumDelays; ++j)
        {
            float8::storeu(&mXNew[j][i], xNew[j]);
        }
    }

    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].put(mFrameSize, mXNew[i]);
    }

    for (auto i = 1; i < kNumDelays; ++i)
    {
        ArrayMath::add(mFrameSize, mXOld[0], mXOld[i], mXOld[0]);
    }

    ArrayMath::scale(mFrameSize, mXOld[0], 1.0f / kNumDelays, mXOld[0]);

    for (auto i = 0; i < mFrameSize; i += 8)
    {
        xOld[0] = float8::loadu(&mXOld[0][i]);

        for (auto j = 0; j < kNumAllpasses; ++j)
        {
            xOld[0] = mAllpass[j].apply(xOld[0]);
        }

        float8::storeu(&out[i], xOld[0]);
    }

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mToneCorrection[i][mCurrent].apply(mFrameSize, out, out);
    }

    float8::avoidTransitionPenalty();
}

void IPL_FLOAT8_ATTR ReverbEffect::multiplyHadamardMatrix(const float8_t* in,
                                          float8_t* out)
{
    out[0]  = in[0] + in[1] + in[2] + in[3] + in[4] + in[5] + in[6] + in[7] + in[8] + in[9] + in[10] + in[11] + in[12] + in[13] + in[14] + in[15];
    out[1]  = in[0] - in[1] + in[2] - in[3] + in[4] - in[5] + in[6] - in[7] + in[8] - in[9] + in[10] - in[11] + in[12] - in[13] + in[14] - in[15];
    out[2]  = in[0] + in[1] - in[2] - in[3] + in[4] + in[5] - in[6] - in[7] + in[8] + in[9] - in[10] - in[11] + in[12] + in[13] - in[14] - in[15];
    out[3]  = in[0] - in[1] - in[2] + in[3] + in[4] - in[5] - in[6] + in[7] + in[8] - in[9] - in[10] + in[11] + in[12] - in[13] - in[14] + in[15];
    out[4]  = in[0] + in[1] + in[2] + in[3] - in[4] - in[5] - in[6] - in[7] + in[8] + in[9] + in[10] + in[11] - in[12] - in[13] - in[14] - in[15];
    out[5]  = in[0] - in[1] + in[2] - in[3] - in[4] + in[5] - in[6] + in[7] + in[8] - in[9] + in[10] - in[11] - in[12] + in[13] - in[14] + in[15];
    out[6]  = in[0] + in[1] - in[2] - in[3] - in[4] - in[5] + in[6] + in[7] + in[8] + in[9] - in[10] - in[11] - in[12] - in[13] + in[14] + in[15];
    out[7]  = in[0] - in[1] - in[2] + in[3] - in[4] + in[5] + in[6] - in[7] + in[8] - in[9] - in[10] + in[11] - in[12] + in[13] + in[14] - in[15];
    out[8]  = in[0] + in[1] + in[2] + in[3] + in[4] + in[5] + in[6] + in[7] - in[8] - in[9] - in[10] - in[11] - in[12] - in[13] - in[14] - in[15];
    out[9]  = in[0] - in[1] + in[2] - in[3] + in[4] - in[5] + in[6] - in[7] - in[8] + in[9] - in[10] + in[11] - in[12] + in[13] - in[14] + in[15];
    out[10] = in[0] + in[1] - in[2] - in[3] + in[4] + in[5] - in[6] - in[7] - in[8] - in[9] + in[10] + in[11] - in[12] - in[13] + in[14] + in[15];
    out[11] = in[0] - in[1] - in[2] + in[3] + in[4] - in[5] - in[6] + in[7] - in[8] + in[9] + in[10] - in[11] - in[12] + in[13] + in[14] - in[15];
    out[12] = in[0] + in[1] + in[2] + in[3] - in[4] - in[5] - in[6] - in[7] - in[8] - in[9] - in[10] - in[11] + in[12] + in[13] + in[14] + in[15];
    out[13] = in[0] - in[1] + in[2] - in[3] - in[4] + in[5] - in[6] + in[7] - in[8] + in[9] - in[10] + in[11] + in[12] - in[13] + in[14] - in[15];
    out[14] = in[0] + in[1] - in[2] - in[3] - in[4] - in[5] + in[6] + in[7] - in[8] - in[9] + in[10] + in[11] + in[12] + in[13] - in[14] - in[15];
    out[15] = in[0] - in[1] - in[2] + in[3] - in[4] + in[5] + in[6] - in[7] - in[8] + in[9] + in[10] - in[11] + in[12] - in[13] - in[14] + in[15];

    for (auto i = 0; i < kNumDelays; ++i)
    {
        out[i] = float8::mul(out[i], float8::set1(0.25f));
    }
}

}

#endif
