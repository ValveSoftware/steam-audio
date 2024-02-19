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

#if defined(IPL_USES_RADEONRAYS)

#include "radeonrays_scene.h"
#include "radeonrays_static_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysScene
// --------------------------------------------------------------------------------------------------------------------

RadeonRaysScene::RadeonRaysScene(shared_ptr<RadeonRaysDevice> radeonRays)
    : mRadeonRays(radeonRays)
{
    mCPUScene = ipl::make_shared<Scene>();
}

RadeonRaysScene::RadeonRaysScene(shared_ptr<RadeonRaysDevice> radeonRays,
                                 const Serialized::Scene* serializedObject)
    : mRadeonRays(radeonRays)
{
    assert(serializedObject);
    assert(serializedObject->static_meshes() && serializedObject->static_meshes()->Length() > 0);

    mCPUScene = ipl::make_shared<Scene>();

    auto numObjects = serializedObject->static_meshes()->Length();

    for (auto i = 0u; i < numObjects; ++i)
    {
        auto staticMesh = ipl::make_shared<RadeonRaysStaticMesh>(radeonRays, serializedObject->static_meshes()->Get(i));
        addStaticMesh(std::static_pointer_cast<IStaticMesh>(staticMesh));
    }

    commit();
}

RadeonRaysScene::RadeonRaysScene(shared_ptr<RadeonRaysDevice> radeonRays,
                                 SerializedObject& serializedObject)
    : RadeonRaysScene(radeonRays, Serialized::GetScene(serializedObject.data()))
{}

shared_ptr<IStaticMesh> RadeonRaysScene::createStaticMesh(int numVertices,
                                                          int numTriangles,
                                                          int numMaterials,
                                                          const Vector3f* vertices,
                                                          const Triangle* triangles,
                                                          const int* materialIndices,
                                                          const Material* materials)
{
    auto staticMesh = ipl::make_shared<RadeonRaysStaticMesh>(mRadeonRays, numVertices, numTriangles, numMaterials,
                                                             vertices, triangles, materialIndices, materials);

    return std::static_pointer_cast<IStaticMesh>(staticMesh);
}

shared_ptr<IStaticMesh> RadeonRaysScene::createStaticMesh(SerializedObject& serializedObject)
{
    auto staticMesh = ipl::make_shared<RadeonRaysStaticMesh>(mRadeonRays, serializedObject);
    return std::static_pointer_cast<IStaticMesh>(staticMesh);
}

void RadeonRaysScene::addStaticMesh(shared_ptr<IStaticMesh> staticMesh)
{
    mStaticMeshes.push_back(staticMesh);

    mCPUScene->addStaticMesh(static_cast<RadeonRaysStaticMesh*>(staticMesh.get())->cpuStaticMesh());
}

void RadeonRaysScene::removeStaticMesh(shared_ptr<IStaticMesh> staticMesh)
{
    mStaticMeshes.remove(staticMesh);

    mCPUScene->removeStaticMesh(static_cast<RadeonRaysStaticMesh*>(staticMesh.get())->cpuStaticMesh());
}

void RadeonRaysScene::commit()
{
    if (numStaticMeshes() > 0)
    {
        mRadeonRays->api()->DetachAll();
        for (const auto& staticMesh : mStaticMeshes)
        {
            mRadeonRays->api()->AttachShape(static_cast<RadeonRaysStaticMesh*>(staticMesh.get())->shape());
        }

        mRadeonRays->api()->Commit();
    }

    mCPUScene->commit();
}

}

#endif
