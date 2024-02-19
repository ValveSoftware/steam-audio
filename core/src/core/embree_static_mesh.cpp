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

#include "embree_static_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeStaticMesh
// --------------------------------------------------------------------------------------------------------------------

EmbreeStaticMesh::EmbreeStaticMesh(shared_ptr<EmbreeScene> scene,
                                   int numVertices,
                                   int numTriangles,
                                   int numMaterials,
                                   const Vector3f* vertices,
                                   const Triangle* triangles,
                                   const int* materialIndices,
                                   const Material* materials)
    : mScene(scene)
    , mNumVertices(numVertices)
    , mNumTriangles(numTriangles)
    , mMaterialIndices(numTriangles)
{
    initialize(*scene, vertices, triangles);

    memcpy(mMaterialIndices.data(), materialIndices, numTriangles * sizeof(int));

    mMaterials.resize(numMaterials);
    memcpy(mMaterials.data(), materials, numMaterials * sizeof(Material));
}

EmbreeStaticMesh::EmbreeStaticMesh(shared_ptr<EmbreeScene> scene,
                                   const Serialized::StaticMesh* serializedObject)
    : mScene(scene)
{
    assert(serializedObject);
    assert(serializedObject->mesh());
    assert(serializedObject->mesh()->vertices() && serializedObject->mesh()->vertices()->Length() > 0);
    assert(serializedObject->mesh()->triangles() && serializedObject->mesh()->triangles()->Length() > 0);
    assert(serializedObject->material_indices() && serializedObject->material_indices()->Length() > 0);
    assert(serializedObject->materials() && serializedObject->materials()->Length() > 0);

    mNumVertices = serializedObject->mesh()->vertices()->Length();
    mNumTriangles = serializedObject->mesh()->triangles()->Length();

    Array<Vector3f> vertices(mNumVertices);
    Array<Triangle> triangles(mNumTriangles);
    memcpy(vertices.data(), serializedObject->mesh()->vertices()->data(), mNumVertices * sizeof(Vector3f));
    memcpy(triangles.data(), serializedObject->mesh()->triangles()->data(), mNumTriangles * sizeof(Triangle));
    initialize(*scene, vertices.data(), triangles.data());

    mMaterialIndices.resize(mNumTriangles);
    memcpy(mMaterialIndices.data(), serializedObject->material_indices()->data(), mNumTriangles * sizeof(int));

    auto numMaterials = serializedObject->materials()->Length();

    mMaterials.resize(numMaterials);
    memcpy(mMaterials.data(), serializedObject->materials()->data(), numMaterials * sizeof(Material));
}

EmbreeStaticMesh::EmbreeStaticMesh(shared_ptr<EmbreeScene> scene,
                                   SerializedObject& serializedObject)
    : EmbreeStaticMesh(scene, Serialized::GetStaticMesh(serializedObject.data()))
{}

EmbreeStaticMesh::~EmbreeStaticMesh()
{
    if (auto scene = mScene.lock())
    {
        rtcDeleteGeometry(scene->scene(), mGeometryIndex);
    }
}

void EmbreeStaticMesh::initialize(const EmbreeScene& scene,
                                  const Vector3f* vertices,
                                  const Triangle* triangles)
{
    mGeometryIndex = rtcNewTriangleMesh(scene.scene(), RTC_GEOMETRY_STATIC, mNumTriangles, mNumVertices, 1);

    auto vertexBuffer = reinterpret_cast<float*>(rtcMapBuffer(scene.scene(), mGeometryIndex, RTC_VERTEX_BUFFER));
    auto indexBuffer = reinterpret_cast<int32_t*>(rtcMapBuffer(scene.scene(), mGeometryIndex, RTC_INDEX_BUFFER));

    for (auto i = 0; i < mNumVertices; ++i)
    {
        vertexBuffer[4 * i + 0] = vertices[i].x();
        vertexBuffer[4 * i + 1] = vertices[i].y();
        vertexBuffer[4 * i + 2] = vertices[i].z();
    }

    memcpy(indexBuffer, triangles, mNumTriangles * sizeof(Triangle));

    rtcUnmapBuffer(scene.scene(), mGeometryIndex, RTC_VERTEX_BUFFER);
    rtcUnmapBuffer(scene.scene(), mGeometryIndex, RTC_INDEX_BUFFER);
}

void EmbreeStaticMesh::enable(const EmbreeScene& scene)
{
    rtcEnable(scene.scene(), mGeometryIndex);
}

void EmbreeStaticMesh::disable(const EmbreeScene& scene)
{
    rtcDisable(scene.scene(), mGeometryIndex);
}

}

#endif
