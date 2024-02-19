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

#include "array.h"
#include "serialized_object.h"
#include "triangle.h"

#include "mesh.fbs.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Mesh
// --------------------------------------------------------------------------------------------------------------------

// A triangle mesh. Vertices are stored in a contiguous array, and the triangles are stored in indexed form. Each
// triangle requires three indices to store (i.e., strip or fan representations are not supported).
class Mesh
{
public:
    Mesh(int numVertices,
         int numTriangles,
         const Vector3f* vertices,
         const Triangle* triangleIndices);

    Mesh(const Serialized::Mesh* serializedObject);

    int numVertices() const
    {
        return static_cast<int>(mVertices.size(0));
    }

    int numTriangles() const
    {
        return static_cast<int>(mTriangles.size(0));
    }

    Vector4f* vertices()
    {
        return mVertices.data();
    }

    const Vector4f* vertices() const
    {
        return mVertices.data();
    }

    Triangle* triangles()
    {
        return mTriangles.data();
    }

    const Triangle* triangles() const
    {
        return mTriangles.data();
    }

    Vector3f& vertex(int i)
    {
        return *reinterpret_cast<Vector3f*>(&mVertices[i]);
    }

    const Vector3f& vertex(int i) const
    {
        return *reinterpret_cast<const Vector3f*>(&mVertices[i]);
    }

    Triangle& triangle(int i)
    {
        return mTriangles[i];
    }

    const Triangle& triangle(int i) const
    {
        return mTriangles[i];
    }

    Vector3f& triangleVertex(int triangleIndex,
                             int vertexIndex)
    {
        return vertex(triangle(triangleIndex).indices[vertexIndex]);
    }

    const Vector3f& triangleVertex(int triangleIndex,
                                   int vertexIndex) const
    {
        return vertex(triangle(triangleIndex).indices[vertexIndex]);
    }

    Vector3f& normal(int i)
    {
        return mNormals[i];
    }

    const Vector3f& normal(int i) const
    {
        return mNormals[i];
    }

    flatbuffers::Offset<Serialized::Mesh> serialize(SerializedObject& serializedObject) const;

private:
    Array<Vector4f> mVertices;
    Array<Triangle> mTriangles;
    Array<Vector3f> mNormals;

    void calcNormals();
};

}
