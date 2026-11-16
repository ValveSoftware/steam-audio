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

// An impulse response whose size is not a multiple of the frame size ends with a partial block. The part of the temporary
// block that the partial block does not fill kept what the previous block had left there, so the convolution of a unit
// impulse repeated samples of the impulse response after its end.
TEST_CASE("The partitioned impulse response ends where it ends when its size is not a multiple of the frame size", "[OverlapSaveConvolutionEffect]")
{
    const auto samplingRate = 48000;
    const auto frameSize = 512;
    const auto irSize = 24000; // 46 blocks and 448 samples

    ImpulseResponse ir(static_cast<float>(irSize) / samplingRate, 0, samplingRate);
    REQUIRE(ir.numSamples() >= irSize);

    std::mt19937 rng(1);
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

    OverlapSaveConvolutionEffect effect(AudioSettings{ samplingRate, frameSize }, OverlapSaveConvolutionEffectSettings{ 1, irSize });

    const auto numFrames = OverlapSaveConvolutionEffect::numBlocks(frameSize, irSize) + 2;
    AudioBuffer in(1, frameSize);
    AudioBuffer out(1, frameSize);

    std::vector<float> x(static_cast<size_t>(numFrames) * frameSize, 0.0f);
    x[0] = 1.0f;
    std::vector<float> y;

    OverlapSaveConvolutionEffectParams params{};
    params.fftIR = &fir;
    params.numChannels = 1;
    params.numSamples = irSize;

    // The first frame after a new impulse response is crossfaded from silence: let it pass.
    memset(in[0], 0, frameSize * sizeof(float));
    effect.apply(params, in, out);

    for (auto frame = 0; frame < numFrames; ++frame)
    {
        memcpy(in[0], &x[static_cast<size_t>(frame) * frameSize], frameSize * sizeof(float));
        effect.apply(params, in, out);
        y.insert(y.end(), out[0], out[0] + frameSize);
    }

    // The output of a unit impulse is the impulse response, then silence.
    auto maxErrorInside = 0.0;
    for (auto i = 0; i < irSize; ++i)
    {
        maxErrorInside = std::max(maxErrorInside, static_cast<double>(std::fabs(y[i] - h[i])));
    }

    auto energyAfter = 0.0;
    for (size_t i = irSize; i < y.size(); ++i)
    {
        energyAfter += static_cast<double>(y[i]) * y[i];
    }

    INFO("largest error inside the impulse response: " << maxErrorInside << ", energy after its end: " << energyAfter);
    CHECK(maxErrorInside < 1e-3);
    CHECK(energyAfter < 1e-6);
}
