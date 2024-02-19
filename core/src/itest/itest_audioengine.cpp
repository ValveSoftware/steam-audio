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
using namespace ipl;

#include "itest.h"

ITEST(audioengine)
{
	auto audio = [&](const AudioBuffer& in, AudioBuffer& out)
	{
		memcpy(out[0], in[0], out.numSamples() * sizeof(float));
		memcpy(out[1], in[1], out.numSamples() * sizeof(float));
	};

	UIWindow window;
	window.run(nullptr, nullptr, audio);
//    auto nullEffect = [](const ipl::AudioBuffer& inBuffer, ipl::AudioBuffer& outBuffer)
//    {
//        if (inBuffer.numChannels() == 1)
//        {
//            for (auto i = 0; i < inBuffer.numSamples(); ++i)
//            {
//                outBuffer[0][i] = inBuffer[0][i];
//                outBuffer[1][i] = inBuffer[0][i];
//            }
//        }
//        else if (inBuffer.numChannels() == 2)
//        {
//            memcpy(outBuffer[0], inBuffer[0], inBuffer.numSamples() * sizeof(float));
//            memcpy(outBuffer[1], inBuffer[1], inBuffer.numSamples() * sizeof(float));
//        }
//    };
//
//    auto balanceEffect = [](const ipl::AudioBuffer& inBuffer, ipl::AudioBuffer& outBuffer)
//    {
//        if (inBuffer.numChannels() == 2)
//        {
//            for (auto i = 0; i < inBuffer.numSamples(); ++i)
//            {
//                outBuffer[0][i] = inBuffer[0][i];
//                outBuffer[1][i] = 0.0f;
//            }
//        }
//    };
//
//    ipl::UIWindow window{ ipl::UIWindow::kShowAudioPlayer, balanceEffect, nullptr };
//    ipl::UILabel sourcesSectionLabel{ "AUDIO SOURCES", ipl::UILabel::Type::Section };
//
//    window.addWidget(sourcesSectionLabel);
//    window.addAudioSource(nullEffect);
//    window.addAudioSource(nullEffect);
//    window.run();
}