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

#pragma once

#include "audio_buffer.h"
#include "bands.h"
#include "delay.h"
#include "iir.h"
#include "matrix.h"
#include "reverb_estimator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ReverbEffect
// --------------------------------------------------------------------------------------------------------------------

struct ReverbEffectParams
{
    const Reverb* reverb = nullptr;
};

class ReverbEffect
{
public:
    ReverbEffect(const AudioSettings& audioSettings);

    void reset();

    AudioEffectState apply(const ReverbEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    AudioEffectState tailApply(const AudioBuffer& in, AudioBuffer& out);

    int numTailSamplesRemaining() const { return mNumTailFramesRemaining * mFrameSize; }

private:
    static constexpr int kNumDelays = 16;
    static constexpr int kNumAllpasses = 4;
    static constexpr float kToneCorrectionWeight = 0.5f;

    int mSamplingRate;
    int mFrameSize;
    int mDelayValues[kNumDelays];
    Delay mDelayLines[kNumDelays];
    int mCurrent;
    bool mIsFirstFrame;
    Allpass mAllpass[kNumAllpasses];
    IIRFilterer mAbsorptive[kNumDelays][Bands::kNumBands][2];
    IIRFilterer mToneCorrection[Bands::kNumBands][2];
    Array<float, 2> mXOld;
    Array<float, 2> mXNew;
    Reverb mPrevReverb;
    int mNumTailFramesRemaining;

    void (ReverbEffect::* mApplyDispatch)(const float* reverbTimes,
                                          const float* in,
                                          float* out);

    void (ReverbEffect::* mTailDispatch)(float* out);

    void calcDelaysForReverbTime(float reverbTime);

    void calcAbsorptiveGains(const float* reverbTimes,
                             int delay,
                             float* gains);

    void calcToneCorrectionGains(const float* reverbTimes,
                                 float* gains);

    void apply_float4(const float* reverbTimes,
                      const float* in,
                      float* out);

    void apply_float8(const float* reverbTimes,
                      const float* in,
                      float* out);

    void tail_float4(float* out);
    void tail_float8(float* out);

    static void multiplyHadamardMatrix(const float4_t* in,
                                       float4_t* out);

#if defined(IPL_ENABLE_FLOAT8)
    static void IPL_FLOAT8_ATTR multiplyHadamardMatrix(const float8_t* in,
                                       float8_t* out);
#endif

    static int nextPowerOfPrime(int x,
                                int p);
};

}
