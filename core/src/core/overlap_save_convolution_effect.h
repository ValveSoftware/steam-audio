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
#include "fft.h"
#include "impulse_response.h"
#include "triple_buffer.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveFIR
// --------------------------------------------------------------------------------------------------------------------

class OverlapSaveFIR
{
public:
    OverlapSaveFIR(int numChannels,
                   int irSize,
                   int frameSize);

    int numChannels() const
    {
        return static_cast<int>(mData.size(0));
    }

    int numBlocks() const
    {
        return static_cast<int>(mData.size(1));
    }

    int numSpectrumSamples() const
    {
        return static_cast<int>(mData.size(2));
    }

    complex_t* const* const* data()
    {
        return mData.data();
    }

    const complex_t* const* const* data() const
    {
        return mData.data();
    }

    complex_t* const* operator[](int i)
    {
        return mData[i];
    }

    const complex_t* const* operator[](int i) const
    {
        return mData[i];
    }

    void reset();

private:
    Array<complex_t, 3> mData;
};


// --------------------------------------------------------------------------------------------------------------------
// OverlapSavePartitioner
// --------------------------------------------------------------------------------------------------------------------

class OverlapSavePartitioner
{
public:
    OverlapSavePartitioner(int frameSize);

    void partition(const ImpulseResponse& ir,
                   int numChannels,
                   int numSamples,
                   OverlapSaveFIR& fftIR);

private:
    int mFrameSize;
    FFT mFFT;
    Array<float> mTempIRBlock;
};


// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveConvolutionEffect
// --------------------------------------------------------------------------------------------------------------------

struct OverlapSaveConvolutionEffectSettings
{
    int numChannels = 0;
    int irSize = 0;

    OverlapSaveConvolutionEffectSettings()
        : numChannels(0)
        , irSize(0)
    {}

    OverlapSaveConvolutionEffectSettings(int numChannels, int irSize)
        : numChannels(numChannels)
        , irSize(irSize)
    {}
};

struct OverlapSaveConvolutionEffectParams
{
    TripleBuffer<OverlapSaveFIR>* fftIR = nullptr;
    int numChannels = 0;
    int numSamples = 0;
};

struct OverlapSaveConvolutionMixerParams
{
    int numChannels = 0;
};

class OverlapSaveConvolutionMixer;

class OverlapSaveConvolutionEffect
{
public:
    OverlapSaveConvolutionEffect(const AudioSettings& audioSettings,
                                 const OverlapSaveConvolutionEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const OverlapSaveConvolutionEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState apply(const OverlapSaveConvolutionEffectParams& params,
                           const AudioBuffer& in,
                           OverlapSaveConvolutionMixer& mixer);

    AudioEffectState tail(AudioBuffer& out);

    AudioEffectState tail(OverlapSaveConvolutionMixer& mixer);

    int numTailSamplesRemaining() const { return mNumTailBlocksRemaining * mFrameSize; }

    static int numBlocks(int frameSize,
                         int irSize);

private:
    int mFrameSize;
    int mIRSize;
    int mNumChannels;
    FFT mFFT;
    Array<float> mDryBlock;
    Array<complex_t, 2> mFFTDryBlocks;
    int mDryBlockIndex;
    Array<complex_t, 2> mFFTWet;
    Array<complex_t, 2> mPrevFFTWet;
    Array<float, 2> mWet;
    Array<float, 2> mPrevWet;
    int mNumTailBlocksRemaining;
    unique_ptr<OverlapSaveFIR> mPrevFFTIR;

    bool apply(const OverlapSaveConvolutionEffectParams& params,
               const AudioBuffer& in);

    void tail();
};


// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveConvolutionMixer
// --------------------------------------------------------------------------------------------------------------------

class OverlapSaveConvolutionMixer
{
public:
    OverlapSaveConvolutionMixer(const AudioSettings& audioSettings,
                                const OverlapSaveConvolutionEffectSettings& effectSettings);

    void reset();

    void apply(const OverlapSaveConvolutionMixerParams& params,
               AudioBuffer& out);

    void mix(const complex_t* const* fftWet,
             const complex_t* const* fftWetPrev);

private:
    int mFrameSize;
    int mNumChannels;
    FFT mFFT;
    Array<complex_t, 2> mFFTWet;
    Array<complex_t, 2> mPrevFFTWet;
    Array<float, 2> mWet;
    Array<float, 2> mPrevWet;
};

}
