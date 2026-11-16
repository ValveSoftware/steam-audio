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


#include <catch.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>

#include <audio_buffer.h>
#include <impulse_response.h>
#include <overlap_save_convolution_effect.h>
#include <triple_buffer.h>

using namespace ipl;

// The tail of the overlap-save convolution has to be what the effect would output if it kept being fed with silence: the
// last input block goes through the whole impulse response, so the tail is numBlocks frames long, not numBlocks - 1.
TEST_CASE("The tail of the overlap-save convolution is what silent input would give", "[OverlapSaveConvolutionEffect]")
{
    const auto samplingRate = 48000;
    const auto frameSize = 512;
    const auto irSize = 4 * frameSize;

    ImpulseResponse ir(static_cast<float>(irSize) / samplingRate, 0, samplingRate);
    std::mt19937 rng(2);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> h(irSize);
    for (auto i = 0; i < irSize; ++i)
    {
        h[i] = dist(rng);
        ir[0][i] = h[i];
    }

    TripleBuffer<OverlapSaveFIR> fir;
    fir.initBuffers(1, irSize, frameSize);
    OverlapSavePartitioner partitioner(frameSize);
    partitioner.partition(ir, 1, irSize, *fir.writeBuffer);
    fir.commitWriteBuffer();

    // A unit impulse in the middle of a frame.
    const auto impulsePosition = frameSize / 2;
    AudioBuffer in(1, frameSize);
    AudioBuffer out(1, frameSize);

    OverlapSaveConvolutionEffectParams params{};
    params.fftIR = &fir;
    params.numChannels = 1;
    params.numSamples = irSize;

    // Reference: the frame with the impulse, then frames of silence passed through apply().
    std::vector<float> reference;
    {
        OverlapSaveConvolutionEffect effect(AudioSettings{ samplingRate, frameSize }, OverlapSaveConvolutionEffectSettings{ 1, irSize });

        // The first frame after a new impulse response is crossfaded from silence: let it pass.
        memset(in[0], 0, frameSize * sizeof(float));
        effect.apply(params, in, out);

        for (auto frame = 0; frame < 8; ++frame)
        {
            memset(in[0], 0, frameSize * sizeof(float));
            if (frame == 0)
            {
                in[0][impulsePosition] = 1.0f;
            }

            effect.apply(params, in, out);
            reference.insert(reference.end(), out[0], out[0] + frameSize);
        }
    }

    // Test: the frame with the impulse, then the frames of tail().
    std::vector<float> withTail;
    {
        partitioner.partition(ir, 1, irSize, *fir.writeBuffer);
        fir.commitWriteBuffer();

        OverlapSaveConvolutionEffect effect(AudioSettings{ samplingRate, frameSize }, OverlapSaveConvolutionEffectSettings{ 1, irSize });

        memset(in[0], 0, frameSize * sizeof(float));
        effect.apply(params, in, out);

        in[0][impulsePosition] = 1.0f;
        effect.apply(params, in, out);
        withTail.insert(withTail.end(), out[0], out[0] + frameSize);

        auto state = AudioEffectState::TailRemaining;
        for (auto i = 0; i < 20 && state == AudioEffectState::TailRemaining; ++i)
        {
            state = effect.tail(out);
            withTail.insert(withTail.end(), out[0], out[0] + frameSize);
        }
    }

    // The reference is the impulse response, delayed by the position of the impulse.
    auto referenceError = 0.0;
    for (auto i = 0; i < irSize; ++i)
    {
        referenceError = std::max(referenceError, static_cast<double>(std::fabs(reference[impulsePosition + i] - h[i])));
    }

    REQUIRE(referenceError < 1e-3);

    auto referenceEnergy = 0.0;
    auto errorEnergy = 0.0;
    for (size_t i = 0; i < withTail.size() && i < reference.size(); ++i)
    {
        referenceEnergy += static_cast<double>(reference[i]) * reference[i];
        errorEnergy += static_cast<double>(withTail[i] - reference[i]) * (withTail[i] - reference[i]);
    }

    INFO("energy of the difference between the tail and the reference, relative to the reference: " << errorEnergy / referenceEnergy);
    CHECK(errorEnergy / referenceEnergy < 1e-6);
}
