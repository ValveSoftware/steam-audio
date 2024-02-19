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

#include "bvh.h"

#include "static_mesh.fbs.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IStaticMesh
// --------------------------------------------------------------------------------------------------------------------

// A static triangle mesh. The geometry of this mesh is assumed to never change at runtime. It is described in
// world-space coordinates. Materials are specified for each triangle.
class IStaticMesh
{
public:
    virtual ~IStaticMesh()
    {}

    virtual int numVertices() const = 0;

    virtual int numTriangles() const = 0;

    virtual int numMaterials() const = 0;
};


// --------------------------------------------------------------------------------------------------------------------
// StaticMesh
// --------------------------------------------------------------------------------------------------------------------

// An IStaticMesh implementation that uses the built-in ray tracer backend.
class StaticMesh : public IStaticMesh
{
public:
    StaticMesh(int numVertices,
               int numTriangles,
               int numMaterials,
               const Vector3f* vertices,
               const Triangle* triangles,
               const int* materialIndices,
               const Material* materials);

    StaticMesh(const Serialized::StaticMesh* serializedObject);

    StaticMesh(SerializedObject& serializedObject);

    virtual int numVertices() const override
    {
        return mMesh.numVertices();
    }

    virtual int numTriangles() const override
    {
        return mMesh.numTriangles();
    }

    virtual int numMaterials() const override
    {
        return static_cast<int>(mMaterials.size(0));
    }

    Mesh& mesh()
    {
        return mMesh;
    }

    const Mesh& mesh() const
    {
        return mMesh;
    }

    Box boundingBox() const
    {
        return mBVH.node(0).boundingBox();
    }

    const int* materialIndices() const
    {
        return mMaterialIndices.data();
    }

    Material* materials()
    {
        return mMaterials.data();
    }

    const Material* materials() const
    {
        return mMaterials.data();
    }

    Hit closestHit(const Ray& ray,
                   float minDistance,
                   float maxDistance) const;

    bool anyHit(const Ray& ray,
                float minDistance,
                float maxDistance) const;

    bool intersectsBox(const Box& box) const;

    flatbuffers::Offset<Serialized::StaticMesh> serialize(SerializedObject& serializedObject) const;

    void serializeAsRoot(SerializedObject& serializedObject) const;

private:
    Mesh mMesh;
    BVH mBVH;
    Array<int> mMaterialIndices;
    Array<Material> mMaterials;
};

}
