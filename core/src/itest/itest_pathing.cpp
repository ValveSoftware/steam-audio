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

#include <ambisonics_binaural_effect.h>
#include <ambisonics_rotate_effect.h>
#include <probe_batch.h>
#include <probe_generator.h>
#include <path_simulator.h>
#include <path_effect.h>
#include <profiler.h>
#include <sh.h>
#include "helpers.h"
#include "ui_window.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

struct PathingVisState
{
    std::vector<Vector3f> from;
    std::vector<Vector3f> to;
    std::vector<UIColor> color;
};

ITEST(pathing)
{
    auto context = make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto scene = loadMesh(context, "simplescene.obj", "simplescene.mtl", SceneType::Default);
    auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(scene)->staticMeshes().front())->mesh();

    AudioSettings audioSettings{};
    audioSettings.samplingRate = 44100;
    audioSettings.frameSize = 1024;

    HRTFSettings hrtfSettings{};
    HRTFDatabase hrtf(hrtfSettings, audioSettings.samplingRate, audioSettings.frameSize);

    Matrix4x4f localToWorldTransform{};
    localToWorldTransform.identity();
    localToWorldTransform *= 80;

    auto spacing = 1.5f;
    auto height = 1.5f;
    ProbeArray probes;
    ProbeGenerator::generateProbes(*scene, localToWorldTransform, ProbeGenerationType::UniformFloor, spacing, height, probes);
    auto numProbes = probes.numProbes();
    printf("%d probes.\n", numProbes);

    auto probeBatch = make_shared<ProbeBatch>();
    probeBatch->addProbeArray(probes);
    probeBatch->commit();

    ProbeManager probeManager;
    probeManager.addProbeBatch(probeBatch);
    probeManager.commit();

    const auto kOrder = 3;
    const auto kNumCoeffs = SphericalHarmonics::numCoeffsForOrder(kOrder);
    const auto kNumVisSamples = 1;
    const auto kProbeVisRadius = 0.0f;
    const auto kProbeVisThreshold = 0.5f;
    const auto kProbeVisRange = INFINITY;
    const auto kProbeVisRangeRealTime = 2.0f * spacing;
    const auto kProbePathRange = INFINITY;
    const auto kNumThreads = 12;
    const auto kSpatializeInPathEffect = true;

    BakedDataIdentifier identifier;
    identifier.variation = BakedDataVariation::Dynamic;
    identifier.type = BakedDataType::Pathing;

    auto pathProgressCallback = [](const float percentComplete,
                                   void*)
    {
        printf("\rGenerating path data (%3d%%)", static_cast<int>(100.0f * percentComplete));
    };

    PathBaker::bake(*scene, identifier, kNumVisSamples, kProbeVisRadius, kProbeVisThreshold, kProbeVisRange,
                    kProbeVisRangeRealTime, kProbePathRange, true, -Vector3f::kYAxis, true, kNumThreads, *probeBatch, pathProgressCallback);

    PathSimulator pathSimulator(*probeBatch, kNumVisSamples, true, -Vector3f::kYAxis);

    auto speakerLayout = SpeakerLayout{SpeakerLayoutType::Stereo};

    PathEffectSettings pathSettings{};
    pathSettings.maxOrder = kOrder;
    pathSettings.spatialize = kSpatializeInPathEffect;
    pathSettings.speakerLayout = &speakerLayout;
    pathSettings.hrtf = &hrtf;

    PathEffect pathEffect(audioSettings, pathSettings);

    auto showMesh = true;
    auto listenerDropped = false;
    auto listenerPositionAsDropped = Vector3f::kZero;
    auto sourcePosition = Vector3f::kZero;
    Array<float> readEQGains(Bands::kNumBands);
    Array<float> readCoeffs(kNumCoeffs);
    Array<float> writeEQGains(Bands::kNumBands);
    Array<float> writeCoeffs(kNumCoeffs);
    Array<float>* eqGains[2] = {&readEQGains, &writeEQGains};
    Array<float>* coeffs[2] = {&readCoeffs, &writeCoeffs};
    Vector3f avgDirection(.0f, .0f, .0f);
    float distanceRatio = 1.0f;
    std::mutex mutex;
    std::atomic<bool> newDataWritten = false;
    const char* dragOptions[] = { "None", "Source", "Listener" };
    int selectedDragOption = 0;
    Vector3f screenPoint(.0f, .0f, .0f);

    bool enableValidation = false;
    bool findAlternatePaths = false;
    bool enablePathVisualization = true;

	auto gui = [&]()
	{
		if (ImGui::Button("Drop Source"))
		{
			sourcePosition = UIWindow::sCamera.origin;
		}

		if (ImGui::Button("Drop Listener"))
		{
			listenerDropped = true;
			listenerPositionAsDropped = UIWindow::sCamera.origin;
		}

        ImGui::Combo("Drag Options", &selectedDragOption, dragOptions, IM_ARRAYSIZE(dragOptions));
		ImGui::Checkbox("Show Mesh", &showMesh);
        ImGui::Text("Distance Ratio: %.4f", distanceRatio );
        ImGui::Checkbox("Interpolate All Source Probes", &PathSimulator::sEnablePathsFromAllSourceProbes);
        ImGui::Checkbox("Enable Validation", &enableValidation);
        ImGui::Checkbox("Find Alternate Paths", &findAlternatePaths);
        ImGui::Checkbox("Enable Path Visualization", &enablePathVisualization);
    };

    memset(readEQGains.flatData(), 0, Bands::kNumBands * sizeof(float));
    memset(readCoeffs.flatData(), 0, kNumCoeffs * sizeof(float));
    memset(writeEQGains.flatData(), 0, Bands::kNumBands * sizeof(float));
    memset(writeCoeffs.flatData(), 0, kNumCoeffs * sizeof(float));

	for (auto i = 0; i < Bands::kNumBands; ++i)
	{
		readEQGains[i] = 1.0f;
	}

    PathingVisState visState{};
    auto visualizeValidationRay = [](Vector3f from,
                                     Vector3f to,
                                     bool occluded,
                                     void* userData)
    {
        auto* visState = reinterpret_cast<PathingVisState*>(userData);
        visState->from.push_back(from);
        visState->to.push_back(to);
        visState->color.push_back((occluded) ? UIColor::kRed : UIColor::kCyan);
    };

    const auto kNumTimes = 10;
    double times[kNumTimes] = { 0.0 };
    auto curTime = 0;
    auto avgTime = 0.0;
    auto minTime = std::numeric_limits<double>::infinity();
    auto maxTime = 0.0;

    auto display = [&]()
    {
        auto listenerPosition = UIWindow::sCamera.origin;

        if (showMesh)
        {
            UIWindow::drawMesh(mesh);

            for (auto i = 0; i < probes.numProbes(); ++i)
            {
				UIWindow::drawPoint(probes[i].influence.center, UIColor::kBlue, 8.0f);
            }
        }

		UIWindow::drawPoint(sourcePosition, UIColor::kRed, 8.0f);
        if (listenerDropped || selectedDragOption != 0)
        {
			UIWindow::drawPoint(listenerPositionAsDropped, UIColor::kGreen, 8.0f);
        }

        for (auto i = 0; i < visState.from.size(); ++i)
        {
            UIWindow::drawLineSegment(visState.from[i], visState.to[i], visState.color[i], 2.0f);
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            auto listener = (listenerDropped || selectedDragOption != 0) ? listenerPositionAsDropped : UIWindow::sCamera.origin;

            Timer timer;
            timer.start();

            if (!newDataWritten)
            {
                ProbeNeighborhood sourceProbes;
                probeManager.getInfluencingProbes(sourcePosition, sourceProbes);
                sourceProbes.checkOcclusion(*scene, sourcePosition);
                sourceProbes.calcWeights(sourcePosition);

                ProbeNeighborhood listenerProbes;
                probeManager.getInfluencingProbes(listener, listenerProbes);
                listenerProbes.checkOcclusion(*scene, listener);
                listenerProbes.calcWeights(listener);

                visState.from.clear();
                visState.to.clear();
                visState.color.clear();

                pathSimulator.findPaths(sourcePosition, listener, *scene, *probeBatch, sourceProbes, listenerProbes, kProbeVisRadius,
                    kProbeVisThreshold, kProbeVisRangeRealTime, kOrder, enableValidation, findAlternatePaths, true, true,
                    eqGains[1]->data(), coeffs[1]->data(), &avgDirection, &distanceRatio,
                    enablePathVisualization ? visualizeValidationRay : (ValidationRayVisualizationCallback) nullptr, &visState);

                newDataWritten = true;
            }

            auto usElapsed = timer.elapsedMicroseconds();

            minTime = std::min( minTime, usElapsed );
            maxTime = std::max( maxTime, usElapsed );
            avgTime = ( avgTime * kNumTimes - times[curTime] + usElapsed ) / kNumTimes;
            times[curTime] = usElapsed;
            curTime = ( curTime + 1 ) % kNumTimes;

            Ray ray{listener, avgDirection};
            UIWindow::drawRay(ray, ipl::UIColor::kMagenta, 2.0f);

            if (UIWindow::dragMode())
            {
                screenPoint = UIWindow::screenToWorld(scene, height);
                UIWindow::drawPoint(screenPoint, UIColor::kYellow, 5.0f);

                if (selectedDragOption == 1)
                    sourcePosition = screenPoint;
                else if (selectedDragOption == 2)
                    listenerPositionAsDropped = screenPoint;
            }

            printf( "\rTime (us): avg %5.2f min %5.2f max %5.2f", avgTime, minTime, maxTime );
        }
    };

    AudioBuffer mono(1, audioSettings.frameSize);
    AudioBuffer ambisonics(SphericalHarmonics::numCoeffsForOrder(kOrder), audioSettings.frameSize);

    AmbisonicsRotateEffectSettings rotateSettings{};
    rotateSettings.maxOrder = kOrder;

    AmbisonicsRotateEffect rotateEffect(audioSettings, rotateSettings);

    AmbisonicsBinauralEffectSettings binauralSettings{};
    binauralSettings.maxOrder = kOrder;
    binauralSettings.hrtf = &hrtf;

    AmbisonicsBinauralEffect binauralEffect(audioSettings, binauralSettings);

    auto processAudio = [&](const AudioBuffer& inBuffer,
							AudioBuffer& outBuffer)
    {
        outBuffer.makeSilent();

        if (newDataWritten)
        {
            std::swap(eqGains[0], eqGains[1]);
            std::swap(coeffs[0], coeffs[1]);

            newDataWritten = false;
        }

        AudioBuffer::downmix(inBuffer, mono);

        PathEffectParams pathParams{};
        pathParams.eqCoeffs = eqGains[0]->data();
        pathParams.shCoeffs = coeffs[0]->data();
        pathParams.order = pathSettings.maxOrder;

        float* shCoeffs = coeffs[0]->data();
        float distanceScale = pathParams.shCoeffs[0] > .0f ? 1.0f / (2.0f * sqrtf( Math::kPi ) * pathParams.shCoeffs[0]) : 1.0f;
        for (int i = 0; i < coeffs[0]->totalSize(); ++i)
            shCoeffs[i] *= distanceScale;

        if (kSpatializeInPathEffect)
        {
            pathParams.binaural = true;
            pathParams.hrtf = &hrtf;
            pathParams.listener = &UIWindow::sCamera;

            pathEffect.apply(pathParams, mono, outBuffer);
        }
        else
        {
            pathEffect.apply(pathParams, mono, ambisonics);

            AmbisonicsRotateEffectParams rotateParams{};
            rotateParams.orientation = &UIWindow::sCamera;
            rotateParams.order = kOrder;

            rotateEffect.apply(rotateParams, ambisonics, ambisonics);

            AmbisonicsBinauralEffectParams binauralParams{};
            binauralParams.hrtf = &hrtf;
            binauralParams.order = kOrder;

            binauralEffect.apply(binauralParams, ambisonics, outBuffer);
        }
    };

    auto processTail = [&](AudioBuffer& out)
    {
        if (kSpatializeInPathEffect)
        {
            return pathEffect.tail(out);
        }
        else
        {
            out.makeSilent();
            return AudioEffectState::TailComplete;
        }
    };

	UIWindow window;
	window.run(gui, display, processAudio, processTail);
}
