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

#include "radeonrays_static_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysStaticMesh
// --------------------------------------------------------------------------------------------------------------------

RadeonRaysStaticMesh::RadeonRaysStaticMesh(shared_ptr<RadeonRaysDevice> radeonRays,
                                           int numVertices,
                                           int numTriangles,
                                           int numMaterials,
                                           const Vector3f* vertices,
                                           const Triangle* triangles,
                                           const int* materialIndices,
                                           const Material* materials)
    : mRadeonRays(radeonRays)
    , mNumVertices(numVertices)
    , mNumTriangles(numTriangles)
    , mNumMaterials(numMaterials)
    , mNormals(make_unique<OpenCLBuffer>(radeonRays->openCL(), numTriangles * sizeof(cl_float4)))
    , mMaterialIndices(make_unique<OpenCLBuffer>(radeonRays->openCL(), numTriangles * sizeof(cl_int)))
    , mMaterials(make_unique<OpenCLBuffer>(radeonRays->openCL(), numMaterials * sizeof(Material)))
{
    initialize(vertices, triangles, materialIndices, materials);

    mCPUStaticMesh = ipl::make_shared<StaticMesh>(numVertices, numTriangles, numMaterials, vertices, triangles,
                                                  materialIndices, materials);
}

RadeonRaysStaticMesh::RadeonRaysStaticMesh(shared_ptr<RadeonRaysDevice> radeonRays,
                                           const Serialized::StaticMesh* serializedObject)
    : mRadeonRays(radeonRays)
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
    Array<int> materialIndices(mNumTriangles);
    memcpy(vertices.data(), serializedObject->mesh()->vertices()->data(), mNumVertices * sizeof(Vector3f));
    memcpy(triangles.data(), serializedObject->mesh()->triangles()->data(), mNumTriangles * sizeof(Triangle));
    memcpy(materialIndices.data(), serializedObject->material_indices()->data(), mNumTriangles * sizeof(int));

    mNumMaterials = serializedObject->materials()->Length();

    Array<Material> materials(mNumMaterials);
    memcpy(materials.data(), serializedObject->materials()->data(), mNumMaterials * sizeof(Material));

    mNormals = make_unique<OpenCLBuffer>(radeonRays->openCL(), mNumTriangles * sizeof(cl_float4));
    mMaterialIndices = make_unique<OpenCLBuffer>(radeonRays->openCL(), mNumTriangles * sizeof(cl_int));
    mMaterials = make_unique<OpenCLBuffer>(radeonRays->openCL(), mNumMaterials * sizeof(Material));

    initialize(vertices.data(), triangles.data(), materialIndices.data(), materials.data());

    mCPUStaticMesh = ipl::make_shared<StaticMesh>(mNumVertices, mNumTriangles, mNumMaterials, vertices.data(),
                                                  triangles.data(), materialIndices.data(), materials.data());
}

RadeonRaysStaticMesh::RadeonRaysStaticMesh(shared_ptr<RadeonRaysDevice> radeonRays,
                                           SerializedObject& serializedObject)
    : RadeonRaysStaticMesh(radeonRays, Serialized::GetStaticMesh(serializedObject.data()))
{}

RadeonRaysStaticMesh::~RadeonRaysStaticMesh()
{
    mRadeonRays->api()->DeleteShape(mShape);
}

void RadeonRaysStaticMesh::initialize(const Vector3f* vertices,
                                      const Triangle* triangles,
                                      const int* materialIndices,
                                      const Material* materials)
{
    mShape = mRadeonRays->api()->CreateMesh(reinterpret_cast<const float*>(vertices), mNumVertices, sizeof(Vector3f),
                                            reinterpret_cast<const int32_t*>(triangles), 0, nullptr, mNumTriangles);

    calcNormals(vertices, triangles);

    clEnqueueWriteBuffer(mRadeonRays->openCL().irUpdateQueue(), mMaterialIndices->buffer(), CL_FALSE, 0,
                         mMaterialIndices->size(), materialIndices, 0, nullptr, nullptr);

    clEnqueueWriteBuffer(mRadeonRays->openCL().irUpdateQueue(), mMaterials->buffer(), CL_TRUE, 0,
                         mMaterials->size(), materials, 0, nullptr, nullptr);
}

void RadeonRaysStaticMesh::calcNormals(const Vector3f* vertices,
                                       const Triangle* triangles)
{
    auto normals = reinterpret_cast<Vector4f*>(clEnqueueMapBuffer(mRadeonRays->openCL().irUpdateQueue(),
                                               mNormals->buffer(), CL_TRUE, CL_MAP_WRITE, 0, mNormals->size(), 0,
                                               nullptr, nullptr, nullptr));

    for (auto i = 0; i < mNumTriangles; ++i)
    {
        const auto& v0 = vertices[triangles[i].indices[0]];
        const auto& v1 = vertices[triangles[i].indices[1]];
        const auto& v2 = vertices[triangles[i].indices[2]];

        const auto n = Vector3f::unitVector(Vector3f::cross(Vector3f::unitVector(v1 - v0), Vector3f::unitVector(v2 - v0)));

        normals[i] = Vector4f(n);
    }

    clEnqueueUnmapMemObject(mRadeonRays->openCL().irUpdateQueue(), mNormals->buffer(), normals, 0, nullptr, nullptr);
}

}

#endif
