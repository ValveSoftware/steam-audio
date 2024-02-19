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

#if defined(IPL_OS_ANDROID)

#include "fft.h"

#include <ffts.h>

#include "array_math.h"
#include "error.h"
#include "float4.h"
#include "log.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// FFT
// --------------------------------------------------------------------------------------------------------------------

struct FFT::State
{
    ffts_plan_t* forwardPlan;
    ffts_plan_t* inversePlan;
};

FFT::FFT(int size,
         FFTDomain domain)
{
    numRealSamples = Math::nextpow2(size);
    numComplexSamples = (domain == FFTDomain::Real) ? (numRealSamples / 2 + 1) : numRealSamples;

    mState = make_unique<State>();

    if (domain == FFTDomain::Real)
    {
        mState->forwardPlan = ffts_init_1d_real(numRealSamples, -1);
        if (!mState->forwardPlan)
        {
            gLog().message(MessageSeverity::Error, "Unable to create FFTS plan (size == %d).", numRealSamples);
            throw Exception(Status::Initialization);
        }

        mState->inversePlan = ffts_init_1d_real(numRealSamples, 1);
        if (!mState->inversePlan)
        {
            ffts_free(mState->forwardPlan);
            gLog().message(MessageSeverity::Error, "Unable to create FFTS plan (size == %d).", numRealSamples);
            throw Exception(Status::Initialization);
        }
    }
    else
    {
        mState->forwardPlan = ffts_init_1d(numRealSamples, -1);
        if (!mState->forwardPlan)
        {
            gLog().message(MessageSeverity::Error, "Unable to create FFTS plan (size == %d).", numRealSamples);
            throw Exception(Status::Initialization);
        }

        mState->inversePlan = ffts_init_1d(numRealSamples, 1);
        if (!mState->inversePlan)
        {
            ffts_free(mState->forwardPlan);
            gLog().message(MessageSeverity::Error, "Unable to create FFTS plan (size == %d).", numRealSamples);
            throw Exception(Status::Initialization);
        }
    }
}

FFT::~FFT()
{
    ffts_free(mState->inversePlan);
    ffts_free(mState->forwardPlan);
}

void FFT::applyForward(const float* signal,
                       complex_t* spectrum) const
{
    ffts_execute(mState->forwardPlan, signal, spectrum);
}

void FFT::applyForward(const complex_t* signal,
                       complex_t* spectrum) const
{
    ffts_execute(mState->forwardPlan, signal, spectrum);
}

void FFT::applyInverse(const complex_t* spectrum,
                       float* signal) const
{
    ffts_execute(mState->inversePlan, spectrum, signal);
    ArrayMath::scale(numRealSamples, signal, 1.0f / numRealSamples, signal);
}

void FFT::applyInverse(const complex_t* spectrum,
                       complex_t* signal) const
{
    ffts_execute(mState->inversePlan, spectrum, signal);
    ArrayMath::scale(2 * numRealSamples, (float*) signal, 1.0f / numRealSamples, (float*) signal);
}

}

#endif
