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

#include "mesh.h"

#include "containers.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Mesh
// --------------------------------------------------------------------------------------------------------------------

Mesh::Mesh(int numVertices,
           int numTriangles,
           const Vector3f* vertices,
           const Triangle* triangleIndices)
    : mVertices(numVertices)
    , mTriangles(numTriangles)
    , mNormals(numTriangles)
{
    for (auto i = 0; i < numVertices; ++i)
    {
        mVertices[i] = Vector4f(vertices[i].x(), vertices[i].y(), vertices[i].z(), 1.0f);
    }

    for (auto i = 0; i < numTriangles; ++i)
    {
        mTriangles[i] = triangleIndices[i];
    }

    calcNormals();
}

Mesh::Mesh(const Serialized::Mesh* serializedObject)
{
    assert(serializedObject);
    assert(serializedObject->vertices() && serializedObject->vertices()->Length() > 0);
    assert(serializedObject->triangles() && serializedObject->triangles()->Length() > 0);

    auto numVertices = serializedObject->vertices()->Length();
    auto numTriangles = serializedObject->triangles()->Length();

    mVertices.resize(numVertices);
    mTriangles.resize(numTriangles);
    mNormals.resize(numTriangles);

    for (auto i = 0u; i < numVertices; ++i)
    {
        auto vertex = serializedObject->vertices()->Get(i);
        mVertices[i] = Vector3f(vertex->x(), vertex->y(), vertex->z());
    }

    memcpy(mTriangles.data(), serializedObject->triangles()->data(), numTriangles * sizeof(Triangle));

    calcNormals();
}

flatbuffers::Offset<Serialized::Mesh> Mesh::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    Serialized::Vector3* verticesBuffer = nullptr;
    auto verticesOffset = fbb.CreateUninitializedVectorOfStructs(numVertices(), &verticesBuffer);
    for (auto i = 0; i < numVertices(); ++i)
    {
        verticesBuffer[i] = Serialized::Vector3(vertex(i).x(), vertex(i).y(), vertex(i).z());
    }

    auto trianglesOffset = fbb.CreateVectorOfStructs(reinterpret_cast<const Serialized::Triangle*>(triangles()), numTriangles());

    return Serialized::CreateMesh(fbb, verticesOffset, trianglesOffset);
}

void Mesh::calcNormals()
{
    for (auto i = 0; i < numTriangles(); ++i)
    {
        const auto& v0 = triangleVertex(i, 0);
        const auto& v1 = triangleVertex(i, 1);
        const auto& v2 = triangleVertex(i, 2);

        mNormals[i] = Vector3f::unitVector(Vector3f::cross(v1 - v0, v2 - v0));
    }
}

}
