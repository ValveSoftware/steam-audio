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
#include <directeffect/DirectSoundEffect.h>
#include <platform/Context.h>
#include <environment/Environment.h>

//============================================================================
// Direct Sound Effect
//============================================================================

TEST_CASE("Direct Sound Effect initial value returned is correct.", "[Direct Sound Effect]")
{
    const int kSamplingRate = 48000;
    const int kFrameSize = 1024;

    auto context = ipl::make_shared<ipl::Context>(nullptr, nullptr, nullptr);
    auto computeDevice = nullptr;
    auto scene = nullptr;
    auto probeManager = nullptr;

    ipl::SimulationSettings settings;
    settings.sceneType = ipl::SceneType::Phonon;
    settings.rays = 8192;
    settings.diffuseSamples = 4096;
    settings.bounces = 4;
    settings.irDuration = 1.0;
    settings.ambisonicsOrder = 1;
    settings.maxConvolutionSources = 1;

    auto environment = ipl::make_shared<ipl::Environment>(context);
    environment->computeDevice = computeDevice;
    environment->settings = settings;
    environment->scene = scene;
    environment->probeManager = probeManager;
    environment->finalize();

    ipl::RenderingSettings renderSettings(kSamplingRate, kFrameSize);
    renderSettings.convolutionType = ipl::ConvolutionType::Phonon;

    ipl::AudioFormat monoFormat(ipl::ChannelLayout::Mono, ipl::ChannelOrder::Deinterleaved);

    auto renderer = ipl::make_shared<ipl::EnvironmentalRenderer>(context, environment, renderSettings, monoFormat, nullptr, nullptr);
    auto directSoundEffect = ipl::make_shared<ipl::DirectSoundEffect>(renderer, monoFormat, monoFormat);

    ipl::AudioBuffer inBuffer(monoFormat, kFrameSize);
    ipl::AudioBuffer outBuffer(monoFormat, kFrameSize);

    std::vector<float> frequencyFactors = { 1.0f, 1.0f, 1.0f };

    {
        float attenuationFactor = .0f;
        for (auto i = 0; i < 100; ++i)
            directSoundEffect->apply(inBuffer, attenuationFactor, frequencyFactors, false, outBuffer);
    }

    {
        float attenuationFactor = 1.0f;
        for (auto i = 0; i < 100; ++i)
            directSoundEffect->apply(inBuffer, attenuationFactor, frequencyFactors, false, outBuffer);
    }

    {
        float attenuationFactor = .0f;
        for (auto i = 0; i < 100; ++i)
            directSoundEffect->apply(inBuffer, attenuationFactor, frequencyFactors, false, outBuffer);
    }
}
