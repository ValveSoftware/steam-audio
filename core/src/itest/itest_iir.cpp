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

#include <implot.h>

#include "itest.h"

struct bands_t
{
    int numBands;
    std::vector<float> lowCutoff;
    std::vector<float> highCutoff;
};

void bands_init_3band(bands_t& bands)
{
    bands.numBands = 3;

    bands.lowCutoff.push_back(0.0f);
    bands.lowCutoff.push_back(800.0f);
    bands.lowCutoff.push_back(8000.0f);

    bands.highCutoff.push_back(800.0f);
    bands.highCutoff.push_back(8000.0f);
    bands.highCutoff.push_back(24000.0f);
}

void bands_init_octave(bands_t& bands)
{
    const float fc[] = {15.625, 31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    const float fd = powf(2.0f, 0.5f);

    bands.numBands = ARRAYSIZE(fc);

    for (auto i = 0; i < bands.numBands; ++i)
    {
        bands.lowCutoff.push_back(fc[i] / fd);
        bands.highCutoff.push_back(fc[i] * fd);
    }

    bands.lowCutoff[0] = 0.0f;
    bands.highCutoff[bands.numBands - 1] = 24000.0f;
}

void bands_init_onethirdoctave(bands_t& bands)
{
    std::vector<float> fc;
    for (auto i = -18; i <= 13; ++i)
    {
        fc.push_back(1000.0f * powf(2.0f, (i / 3.0f)));
    }

    const float fd = powf(2.0f, (1.0f / 6.0f));

    bands.numBands = fc.size();

    for (auto i = 0; i < bands.numBands; ++i)
    {
        bands.lowCutoff.push_back(fc[i] / fd);
        bands.highCutoff.push_back(fc[i] * fd);
    }

    bands.lowCutoff[0] = 0.0f;
    bands.highCutoff[bands.numBands - 1] = 24000.0f;
}

ITEST(iir)
{
    {
        int samplingRates[] = {44100, 48000, 24000, 22050, 11025};

        for (auto i = 0; i < ARRAYSIZE(samplingRates); ++i)
        {
            printf("%d\n", samplingRates[i]);
            for (auto j = 1; j < Bands::kNumBands - 1; ++j)
            {
                IIR8 filter = IIR8::bandPass(Bands::kLowCutoffFrequencies[j], Bands::kHighCutoffFrequencies[j], samplingRates[i]);
                printf("%d: %f\n", j, filter.spectrumPeak());
            }
            printf("\n");
        }
    }

    const auto kSamplingRate = 48000;
    const auto kNumSpectrumSamples = 10000;

    bands_t bands[3];
    bands_init_3band(bands[0]);
    bands_init_octave(bands[1]);
    bands_init_onethirdoctave(bands[2]);

    auto type = 0;
    auto bandsType = 0;
    auto band = 0;
    auto gain = 1.0f;
    auto order8 = false;

    auto prevType = -1;
    auto prevBandsType = 0;
    auto prevBand = band;
    auto prevGain = gain;
    auto prevOrder8 = order8;

    IIR filter;
    IIR8 filter8;
    float spectrum[kNumSpectrumSamples];

    ImPlot::CreateContext();

    auto gui = [&]()
    {
        const char* types[] = {
            "Pass",
            "EQ"
        };

        ImGui::Combo("Type", &type, types, 2);

        const char* bandsTypes[] = {
            "3-band",
            "Octave",
            "One-Third Octave"
        };

        ImGui::Combo("Bands", &bandsType, bandsTypes, 3);

        ImGui::SliderInt("Band", &band, 0, bands[bandsType].numBands - 1);
        band = std::min(band, bands[bandsType].numBands - 1);

        if (type == 1)
        {
            ImGui::SliderFloat("Gain", &gain, 0.0f, 1.0f);
        }

        ImGui::Checkbox("8th Order", &order8);

        if (type != prevType ||
            bandsType != prevBandsType ||
            band != prevBand ||
            gain != prevGain ||
            order8 != prevOrder8)
        {
            if (order8)
            {
                if (band == 0)
                {
                    if (type == 0)
                    {
                        filter8 = IIR8::lowPass(bands[bandsType].highCutoff[band], kSamplingRate);
                    }
                    else
                    {
                        filter8 = IIR8::lowShelf(bands[bandsType].highCutoff[band], gain, kSamplingRate);
                    }
                }
                else if (band == bands[bandsType].numBands - 1)
                {
                    if (type == 0)
                    {
                        filter8 = IIR8::highPass(bands[bandsType].lowCutoff[band], kSamplingRate);
                    }
                    else
                    {
                        filter8 = IIR8::highShelf(bands[bandsType].lowCutoff[band], gain, kSamplingRate);
                    }
                }
                else
                {
                    if (type == 0)
                    {
                        filter8 = IIR8::bandPass(bands[bandsType].lowCutoff[band], bands[bandsType].highCutoff[band], kSamplingRate);
                    }
                    else
                    {
                        filter8 = IIR8::peaking(bands[bandsType].lowCutoff[band], bands[bandsType].highCutoff[band], gain, kSamplingRate);
                    }
                }

                for (auto i = 0; i < kNumSpectrumSamples; ++i)
                {
                    spectrum[i] = filter8.spectrum(i * Math::kPi / kNumSpectrumSamples);
                }
            }
            else
            {
                if (band == 0)
                {
                    if (type == 0)
                    {
                        filter = IIR::lowPass(bands[bandsType].highCutoff[band], kSamplingRate);
                    }
                    else
                    {
                        filter = IIR::lowShelf(bands[bandsType].highCutoff[band], gain, kSamplingRate);
                    }
                }
                else if (band == bands[bandsType].numBands - 1)
                {
                    if (type == 0)
                    {
                        filter = IIR::highPass(bands[bandsType].lowCutoff[band], kSamplingRate);
                    }
                    else
                    {
                        filter = IIR::highShelf(bands[bandsType].lowCutoff[band], gain, kSamplingRate);
                    }
                }
                else
                {
                    if (type == 0)
                    {
                        filter = IIR::bandPass(bands[bandsType].lowCutoff[band], bands[bandsType].highCutoff[band], kSamplingRate);
                    }
                    else
                    {
                        filter = IIR::peaking(bands[bandsType].lowCutoff[band], bands[bandsType].highCutoff[band], gain, kSamplingRate);
                    }
                }

                for (auto i = 0; i < kNumSpectrumSamples; ++i)
                {
                    spectrum[i] = filter.spectrum(i * Math::kPi / kNumSpectrumSamples);
                }
            }
        }

        //ImGui::PlotLines("Spectrum", spectrum, kNumSpectrumSamples, 0, nullptr, -1.0f, 2.0f, ImVec2(512, 512));

        if (ImPlot::BeginPlot("Spectrum"))
        {
            ImPlot::PlotLine("Spectrum", spectrum, kNumSpectrumSamples);
            ImPlot::EndPlot();
        }

        prevType = type;
        prevBandsType = bandsType;
        prevBand = band;
        prevGain = gain;
        prevOrder8 = order8;
    };

    UIWindow window;
    window.run(gui, nullptr, nullptr);

    ImPlot::DestroyContext();
}