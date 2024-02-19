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

#include "scene_factory.h"

#include "embree_scene.h"
#include "radeonrays_scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SceneFactory
// --------------------------------------------------------------------------------------------------------------------

unique_ptr<IScene> SceneFactory::create(SceneType type,
                                        ClosestHitCallback closestHitCallback,
                                        AnyHitCallback anyHitCallback,
                                        BatchedClosestHitCallback batchedClosestHitCallback,
                                        BatchedAnyHitCallback batchedAnyHitCallback,
                                        void* userData,
                                        shared_ptr<EmbreeDevice> embree,
                                        shared_ptr<RadeonRaysDevice> radeonRays)
{
    switch (type)
    {
    case SceneType::Default:
        return ipl::make_unique<Scene>();

    case SceneType::Custom:
        return ipl::make_unique<CustomScene>(closestHitCallback, anyHitCallback, batchedClosestHitCallback,
                                             batchedAnyHitCallback, userData);

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    case SceneType::Embree:
        return ipl::make_unique<EmbreeScene>(embree);
#endif

#if defined(IPL_USES_RADEONRAYS)
    case SceneType::RadeonRays:
        return ipl::make_unique<RadeonRaysScene>(radeonRays);
#endif

    default:
        throw Exception(Status::Initialization);
    }
}

unique_ptr<IScene> SceneFactory::create(SceneType type,
                                        shared_ptr<EmbreeDevice> embree,
                                        shared_ptr<RadeonRaysDevice> radeonRays,
                                        SerializedObject& serializedObject)
{
    switch (type)
    {
    case SceneType::Default:
        return ipl::make_unique<Scene>(serializedObject);

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    case SceneType::Embree:
        return ipl::make_unique<EmbreeScene>(embree, serializedObject);
#endif

#if defined(IPL_USES_RADEONRAYS)
    case SceneType::RadeonRays:
        return ipl::make_unique<RadeonRaysScene>(radeonRays, serializedObject);
#endif

    default:
        throw Exception(Status::Initialization);
    }
}

}
