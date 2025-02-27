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

#include "energy_field.h"
#include "eq_effect.h"
#include "gain_effect.h"
#include "impulse_response.h"
#include "reverb_effect.h"
#include "reverb_estimator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HybridReverbEstimator
// --------------------------------------------------------------------------------------------------------------------

class HybridReverbEstimator
{
public:
    HybridReverbEstimator(float maxDuration,
                          int samplingRate,
                          int frameSize);

    void estimate(const EnergyField* energyField,
                  const Reverb& reverb,
                  ImpulseResponse& impulseResponse,
                  float transitionTime,
                  float overlapFraction,
                  int order,
                  float* eqCoeffs,
                  int& delay);

private:
    float mMaxDuration;
    int mSamplingRate;
    int mFrameSize;
    GainEffect mGainEffect;
    EQEffect mEQEffect;
    ReverbEffect mReverbEffect;
    Array<float, 2> mTempFrame;
    Array<float> mReverbIR;
    IIRFilterer mBandpassFilters[Bands::kNumBands];

    int estimateDelay(float transitionTime, float overlapFraction);

    void calcReverbIR(int numSamples,
                      const float* eqCoeffs,
                      const float* reverbTimes);

    void estimateHybridEQFromIR(const ImpulseResponse& ir,
                                float transitionTime,
                                float overlapFraction,
                                int samplingRate,
                                float* bandIR,
                                float* eq);

    static float calcRelativeEDC(const float* ir,
                                 int numSamples,
                                 int cutoffSample);
};


}
