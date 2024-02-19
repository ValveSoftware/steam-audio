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
#include <window_function.h>
using namespace ipl;

#include "itest.h"

ITEST(windowfunction)
{
    const auto frameSize = 1024;
    const auto overlapSize = frameSize / 4;
    const auto tukeySize = frameSize + overlapSize;

    float rectangular[frameSize];
    float bartlett[frameSize];
    float hann[frameSize];
    float hamming[frameSize];
    float blackman[frameSize];
    float blackmanHarris[frameSize];
    float tukey[tukeySize];

    WindowFunction::rectangular(frameSize, rectangular);
    WindowFunction::bartlett(frameSize, bartlett);
    WindowFunction::hann(frameSize, hann);
    WindowFunction::hamming(frameSize, hamming);
    WindowFunction::blackman(frameSize, blackman);
    WindowFunction::blackmanHarris(frameSize, blackmanHarris);
    WindowFunction::tukey(frameSize, overlapSize, tukey);

    auto gui = [&]()
    {
        if (ImGui::Begin("Window Functions"))
        {
            const char* types[] = {
                "Rectangular",
                "Bartlett",
                "Hann",
                "Hamming",
                "Blackman",
                "Blackman-Harris",
                "Tukey"
            };

            const float* data[] = {
                rectangular,
                bartlett,
                hann,
                hamming,
                blackman,
                blackmanHarris,
                tukey
            };

            static auto currentType = -1;
            ImGui::Combo("Type", &currentType, types, 7);

            if (0 <= currentType && currentType < 7)
            {
                auto size = (currentType == 6) ? tukeySize : frameSize;
                ImGui::PlotLines(types[currentType], data[currentType], size, 0, nullptr, 0.0f, 1.0f, ImVec2(512, 512));
            }
        }

        ImGui::End();
    };

    UIWindow window;

    window.run(gui, nullptr, nullptr);
}