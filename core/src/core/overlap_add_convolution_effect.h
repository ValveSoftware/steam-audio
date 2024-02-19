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

#include "array.h"
#include "audio_buffer.h"
#include "fft.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OverlapAddConvolutionEffect
// --------------------------------------------------------------------------------------------------------------------

struct OverlapAddConvolutionEffectSettings
{
    int numChannels = 0;
    int irSize = 0;
};

struct OverlapAddConvolutionEffectParams
{
    const complex_t* const* fftIR = nullptr;
    bool multipleInputs = false;
};

class OverlapAddConvolutionEffect
{
public:
    OverlapAddConvolutionEffect(const AudioSettings& audioSettings,
                                const OverlapAddConvolutionEffectSettings& effectSettings);

    int wetAudioSize() const
    {
        return mFFT.numRealSamples;
    }

    int irSpectrumSize() const
    {
        return mFFT.numComplexSamples;
    }

    void reset();

    AudioEffectState apply(const OverlapAddConvolutionEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const { return mNumTailSamplesRemaining; }

private:
    int mNumChannels;
    int mIRSize;
    int mFrameSize;
    Array<float> mWindow;
    FFT mFFT;
    Array<float> mWindowedDry;
    Array<complex_t> mFFTWindowedDry;
    Array<float, 2> mDry;
    Array<complex_t, 2> mFFTWet;
    Array<float, 2> mWet;
    Array<float, 2> mOverlap;
    int mNumTailSamplesRemaining;
};

}