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
#include <scene_factory.h>
using namespace ipl;

#include <phonon.h>

#include "itest.h"

ITEST(bvh)
{
    auto context = std::make_shared<Context>(nullptr, nullptr, nullptr, SIMDLevel::AVX2, STEAMAUDIO_VERSION);
    auto scene = loadMesh(context, "teapot.obj", "teapot.mtl", SceneType::Default);
    auto& mesh = std::static_pointer_cast<StaticMesh>(std::static_pointer_cast<Scene>(scene)->staticMeshes().front())->mesh();
    auto bvh = make_unique<BVH>(mesh);

    auto display = [&]()
    {
        UIWindow::drawMesh(mesh);

        for (auto i = 0; i < bvh->numNodes(); ++i)
            UIWindow::drawBox(bvh->node(i).boundingBox(), UIColor::kRed);
    };

    UIWindow window;
    UIWindow::sMovementSpeed = 100.0f;

    window.run(nullptr, display, nullptr);
}