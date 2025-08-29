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
    mMaterialsToUpdate.resize(numMaterials);
    memcpy(mMaterials.data(), materials, numMaterials * sizeof(Material));
    memcpy(mMaterialsToUpdate.data(), materials, numMaterials * sizeof(Material));

    convertMaterials();
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

#if defined(IPL_ENABLE_OCTAVE_BANDS)
    // Only deserialize N-band materials.
    auto numMaterials = serializedObject->materials2()->Length();

    mMaterials.resize(numMaterials);
    for (auto i = 0u; i < numMaterials; ++i)
    {
        mMaterials[i].scattering = serializedObject->materials2()->Get(i)->scattering();

        // Assume that we've serialized the correct number of bands.
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mMaterials[i].absorption[j] = serializedObject->materials2()->Get(i)->absorption()->Get(j);
            mMaterials[i].transmission[j] = serializedObject->materials2()->Get(i)->transmission()->Get(j);
        }
    }
#else
    // Only deserialize 3-band materials.
    auto numMaterials = serializedObject->materials()->Length();

    mMaterials.resize(numMaterials);
    mMaterialsToUpdate.resize(numMaterials);
    memcpy(mMaterials.data(), serializedObject->materials()->data(), numMaterials * sizeof(Material));
    memcpy(mMaterialsToUpdate.data(), mMaterials.data(), numMaterials * sizeof(Material));
#endif

    convertMaterials();
}

EmbreeStaticMesh::EmbreeStaticMesh(shared_ptr<EmbreeScene> scene,
                                   SerializedObject& serializedObject)
    : EmbreeStaticMesh(scene, Serialized::GetStaticMesh(serializedObject.data()))
{}

EmbreeStaticMesh::~EmbreeStaticMesh()
{
    if (auto scene = mScene.lock())
    {
        scene->releaseGeometryID(mGeometryIndex);
        rtcReleaseGeometry(mGeometry);
    }
}

void EmbreeStaticMesh::initialize(const EmbreeScene& scene,
                                  const Vector3f* vertices,
                                  const Triangle* triangles)
{
    auto device = rtcGetSceneDevice(scene.scene());
    mGeometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
    rtcSetGeometryBuildQuality(mGeometry, RTC_BUILD_QUALITY_HIGH);
    rtcReleaseDevice(device);

    auto vertexBuffer = reinterpret_cast<float*>(rtcSetNewGeometryBuffer(mGeometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 4 * sizeof(float), mNumVertices));
    auto indexBuffer = reinterpret_cast<int32_t*>(rtcSetNewGeometryBuffer(mGeometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(uint32_t), mNumTriangles));

    for (auto i = 0; i < mNumVertices; ++i)
    {
        vertexBuffer[4 * i + 0] = vertices[i].x();
        vertexBuffer[4 * i + 1] = vertices[i].y();
        vertexBuffer[4 * i + 2] = vertices[i].z();
    }

    for (auto i = 0; i < mNumTriangles; ++i)
    {
        indexBuffer[3 * i + 0] = triangles[i].indices[0];
        indexBuffer[3 * i + 1] = triangles[i].indices[1];
        indexBuffer[3 * i + 2] = triangles[i].indices[2];
    }

    rtcCommitGeometry(mGeometry);

    mGeometryIndex = const_cast<EmbreeScene&>(scene).acquireGeometryID();

    rtcAttachGeometryByID(scene.scene(), mGeometry, mGeometryIndex);
}

void EmbreeStaticMesh::convertMaterials()
{
    mISPCMaterials.resize(mMaterials.size(0));

    for (auto i = 0; i < mMaterials.size(0); ++i)
    {
        mISPCMaterials[i].absorption = mMaterials[i].absorption;
        mISPCMaterials[i].scattering = mMaterials[i].scattering;
        mISPCMaterials[i].transmission = mMaterials[i].transmission;
    }
}

void EmbreeStaticMesh::enable(const EmbreeScene& scene)
{
    rtcEnableGeometry(mGeometry);
}

void EmbreeStaticMesh::disable(const EmbreeScene& scene)
{
    rtcDisableGeometry(mGeometry);
}

}

#endif
