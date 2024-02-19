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
#include "helpers.h"
#include <binaural_effect.h>
#include <direct_simulator.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(directsimulator)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    auto scene = loadMesh(context, "sponza.obj", "sponza.mtl", SceneType::Default);
    auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(scene)->staticMeshes().front())->mesh();

    auto sourcePosition = Vector3f::kZero;
    auto sourceAhead = -Vector3f::kZAxis;
    auto sourceUp = Vector3f::kYAxis;
    auto sourceCoordinates = CoordinateSpace3f{ sourceAhead, sourceUp, sourcePosition };
    auto sourceDirectivity = Directivity{ 0.5f, 1.0f };
    auto sourceRadius = 1.0f;

    auto directSimulator = make_shared<DirectSimulator>(128);

    auto display = [&]()
    {
        UIWindow::drawPoint(sourcePosition, UIColor{ 1.0f, 0.0f, 0.0f });
        UIWindow::drawMesh(mesh);
    };

    HRTFSettings hrtfSettings{};
    HRTFDatabase hrtf(hrtfSettings, audioSettings.samplingRate, audioSettings.frameSize);

    BinauralEffectSettings binauralSettings{};
    binauralSettings.hrtf = &hrtf;

    BinauralEffect binauralEffect(audioSettings, binauralSettings);

    AudioBuffer mono(1, audioSettings.frameSize);

    auto processAudio = [&](const AudioBuffer& inBuffer, AudioBuffer& outBuffer)
    {
        auto& listener = UIWindow::sCamera;
        DirectSoundPath directSoundPath;

        auto flags = (DirectSimulationFlags) (CalcDistanceAttenuation | CalcAirAbsorption | CalcDirectivity | CalcOcclusion);

        directSimulator->simulate(scene.get(), flags, sourceCoordinates, listener, DistanceAttenuationModel{}, AirAbsorptionModel{}, Directivity(0.5f, 1.0f), OcclusionType::Volumetric, 1.0f, 128, 1, directSoundPath);

        auto amplitude = directSoundPath.distanceAttenuation * directSoundPath.occlusion * directSoundPath.directivity;

        AudioBuffer::downmix(inBuffer, mono);

        for (auto i = 0; i < audioSettings.frameSize; ++i)
        {
            mono[0][i] *= amplitude;
        }

        auto direction = UIWindow::sCamera.transformDirectionFromWorldToLocal(sourcePosition);
        if (direction.length() <= Vector3f::kNearlyZero)
        {
            direction = -Vector3f::kZAxis;
        }

        BinauralEffectParams binauralParams{};
        binauralParams.direction = &direction;
        binauralParams.interpolation = HRTFInterpolation::Bilinear;
        binauralParams.hrtf = &hrtf;

        binauralEffect.apply(binauralParams, mono, outBuffer);
    };

	UIWindow window;
	window.run(nullptr, display, processAudio);
}