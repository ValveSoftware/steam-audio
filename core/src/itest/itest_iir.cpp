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

#include "ui_window.h"
#include <math_functions.h>
#include <iir.h>
using namespace ipl;

#include "itest.h"

ITEST(iir)
{
    const auto kSamplingRate = 48000;
    const auto kNumSpectrumSamples = 100;

    auto type = -1;
    auto lowShelfGain = 1.0f;
    auto highShelfGain = 1.0f;
    auto peakingGain = 1.0f;

    auto prevType = type;
    auto prevLowShelfGain = lowShelfGain;
    auto prevHighShelfGain = highShelfGain;
    auto prevPeakingGain = peakingGain;

    IIR filter;
    float spectrum[kNumSpectrumSamples];

    auto gui = [&]()
    {
        const char* types[] = {
            "Low Pass",
            "High Pass",
            "Band Pass",
            "Low Shelf",
            "High Shelf",
            "Peaking"
        };

        ImGui::Combo("Type", &type, types, 6);

        switch (type)
        {
        case 3:
            ImGui::SliderFloat("Gain", &lowShelfGain, 0.0f, 1.0f);
            break;
        case 4:
            ImGui::SliderFloat("Gain", &highShelfGain, 0.0f, 1.0f);
            break;
        case 5:
            ImGui::SliderFloat("Gain", &peakingGain, 0.0f, 1.0f);
            break;
        }

        if (type != prevType ||
            lowShelfGain != prevLowShelfGain ||
            highShelfGain != prevHighShelfGain ||
            peakingGain != prevPeakingGain)
        {
            switch (type)
            {
            case 0:
                filter = IIR::lowPass(800.0f, kSamplingRate);
                break;
            case 1:
                filter = IIR::highPass(8000.0f, kSamplingRate);
                break;
            case 2:
                filter = IIR::bandPass(800.0f, 8000.0f, kSamplingRate);
                break;
            case 3:
                filter = IIR::lowShelf(800.0f, lowShelfGain, kSamplingRate);
                break;
            case 4:
                filter = IIR::highShelf(8000.0f, highShelfGain, kSamplingRate);
                break;
            case 5:
                filter = IIR::peaking(800.0f, 8000.0f, peakingGain, kSamplingRate);
                break;
            }

            for (auto i = 0; i < kNumSpectrumSamples; ++i)
            {
                spectrum[i] = filter.spectrum(i * Math::kPi / kNumSpectrumSamples);
            }
        }

        ImGui::PlotLines("Spectrum", spectrum, kNumSpectrumSamples, 0, nullptr, 0.0f, 1.0f, ImVec2(512, 512));

        prevType = type;
        prevLowShelfGain = lowShelfGain;
        prevHighShelfGain = highShelfGain;
        prevPeakingGain = peakingGain;
    };

    UIWindow window;
    window.run(gui, nullptr, nullptr);
}