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
#include <probe_batch.h>
#include <probe_generator.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(probes)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);

    auto scene = loadMesh(context, "simplescene.obj", "simplescene.mtl", SceneType::Default);
    auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(scene)->staticMeshes().front())->mesh();

    Matrix4x4f localToWorldTransform;
    localToWorldTransform.identity();
    localToWorldTransform *= 80;

    ProbeArray probes;

    const auto kGenerationType = ProbeGenerationType::Octree;
    const auto kSpacing = 2.0f;
    const auto kHeightAboveFloor = 1.5f;

    ProbeGenerator::generateProbes(*scene, localToWorldTransform, kGenerationType, kSpacing, kHeightAboveFloor, probes);
    printf("numProbes = %zd\n", probes.probes.size(0));

    ProbeBatch probeBatch;
    for (auto i = 0; i < probes.probes.size(0); ++i)
    {
        probeBatch.addProbe(probes.probes[i].influence);
    }

    probeBatch.commit();

    ProbeNeighborhood neighborhood;
    neighborhood.resize(8);

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);

        auto testPoint = UIWindow::sCamera.origin + UIWindow::sCamera.ahead * 5.0f;
        UIWindow::drawPoint(testPoint, UIColor{ 0.0f, 0.0f, 1.0f }, 2.0f);

        neighborhood.reset();
        probeBatch.getInfluencingProbes(testPoint, neighborhood);

        for (auto i = 0; i < probes.probes.size(0); ++i)
        {
            bool isClosest = false;
            for (auto j = 0; j < 8; ++j)
            {
                if (neighborhood.probeIndices[j] == i)
                {
                    isClosest = true;
                }
            }

            if (isClosest)
            {
                UIWindow::drawPoint(probeBatch[i].influence.center, UIColor::kGreen, 2.0f);
            }
            else
            {
                UIWindow::drawPoint(probeBatch[i].influence.center, UIColor::kBlack, 2.0f);
            }
        }
    };

    UIWindow window;
    window.run(nullptr, display, nullptr);
}