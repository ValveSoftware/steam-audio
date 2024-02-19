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

#pragma once

#include "custom_scene.h"
#include "embree_device.h"
#include "radeonrays_device.h"
#include "scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SceneFactory
// --------------------------------------------------------------------------------------------------------------------

enum class SceneType
{
    Default,
    Embree,
    RadeonRays,
    Custom
};

namespace SceneFactory
{
    unique_ptr<IScene> create(SceneType type,
                              ClosestHitCallback closestHitCallback,
                              AnyHitCallback anyHitCallback,
                              BatchedClosestHitCallback batchedClosestHitCallback,
                              BatchedAnyHitCallback batchedAnyHitCallback,
                              void* userData,
                              shared_ptr<EmbreeDevice> embree,
                              shared_ptr<RadeonRaysDevice> radeonRays);

    unique_ptr<IScene> create(SceneType type,
                              shared_ptr<EmbreeDevice> embree,
                              shared_ptr<RadeonRaysDevice> radeonRays,
                              SerializedObject& serializedObject);
}

}
