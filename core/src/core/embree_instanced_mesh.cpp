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

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#include "embree_instanced_mesh.h"
#include "embree_static_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeInstancedMesh
// --------------------------------------------------------------------------------------------------------------------

EmbreeInstancedMesh::EmbreeInstancedMesh(shared_ptr<EmbreeScene> scene,
                                         shared_ptr<EmbreeScene> subScene,
                                         const Matrix4x4f& transform)
    : mScene(scene)
    , mSubScene(subScene)
    , mNumVertices(0)
    , mNumTriangles(0)
    , mHasChanged(false)
    , mObjectIndex(-1)
{
    for (const auto& mesh : mSubScene->staticMeshes())
    {
        const auto& embreeStaticMesh = (EmbreeStaticMesh*) mesh.get();
        mNumVertices += embreeStaticMesh->numVertices();
        mNumTriangles += embreeStaticMesh->numTriangles();
    }

    mSubScene->commit();

    auto device = rtcGetSceneDevice(scene->scene());
    mGeometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
    rtcSetGeometryInstancedScene(mGeometry, subScene->scene());
    rtcReleaseDevice(device);

    updateTransform(*scene, transform);

    mInstanceIndex = scene->acquireGeometryID();

    rtcAttachGeometryByID(scene->scene(), mGeometry, mInstanceIndex);
}

EmbreeInstancedMesh::~EmbreeInstancedMesh()
{
    if (auto scene = mScene.lock())
    {
        scene->releaseGeometryID(mInstanceIndex);
        rtcReleaseGeometry(mGeometry);
    }
}

void EmbreeInstancedMesh::updateTransform(const IScene& scene,
                                          const Matrix4x4f& transform)
{
    const auto& _scene = static_cast<const EmbreeScene&>(scene);

    // If the elements of the transform matrix have changed, consider this instanced mesh to have changed since the
    // last call to commit().
    if (memcmp(transform.elements, mTransform.elements, 16 * sizeof(float)) != 0)
    {
        mHasChanged = true;
    }

    mTransform = transform;
    rtcSetGeometryTransform(mGeometry, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, mTransform.elements);
}

void EmbreeInstancedMesh::commit(const IScene& scene)
{
    const auto& _scene = static_cast<const EmbreeScene&>(scene);

    rtcCommitGeometry(mGeometry);

    // After calling commit(), this instanced mesh will be considered unchanged until a subsequent call to
    // updateTransform() changes the transform matrix.
    mHasChanged = false;
}

bool EmbreeInstancedMesh::hasChanged() const
{
    return mHasChanged;
}

void EmbreeInstancedMesh::enable(const EmbreeScene& scene)
{
    rtcEnableGeometry(mGeometry);
}

void EmbreeInstancedMesh::disable(const EmbreeScene& scene)
{
    rtcDisableGeometry(mGeometry);
}

}

#endif
