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

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#include "embree_scene.h"
#include "static_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeStaticMesh
// --------------------------------------------------------------------------------------------------------------------

// An IStaticMesh implementation that uses Embree as its ray tracer backend.
class EmbreeStaticMesh : public IStaticMesh
{
public:
    EmbreeStaticMesh(shared_ptr<EmbreeScene> scene,
                     int numVertices,
                     int numTriangles,
                     int numMaterials,
                     const Vector3f* vertices,
                     const Triangle* triangles,
                     const int* materialIndices,
                     const Material* materials);

    EmbreeStaticMesh(shared_ptr<EmbreeScene> scene,
                     const Serialized::StaticMesh* serializedObject);

    EmbreeStaticMesh(shared_ptr<EmbreeScene> scene,
                     SerializedObject& serializedObject);

    virtual ~EmbreeStaticMesh() override;

    virtual int numVertices() const override
    {
        return mNumVertices;
    }

    virtual int numTriangles() const override
    {
        return mNumTriangles;
    }

    virtual int numMaterials() const override
    {
        return static_cast<int>(mMaterials.size(0));
    }

    uint32_t geometryIndex() const
    {
        return mGeometryIndex;
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

    void enable(const EmbreeScene& scene);

    void disable(const EmbreeScene& scene);

private:
    void initialize(const EmbreeScene& scene,
                    const Vector3f* vertices,
                    const Triangle* triangles);

    std::weak_ptr<EmbreeScene> mScene;
    uint32_t mGeometryIndex;
    int mNumVertices;
    int mNumTriangles;
    Array<int> mMaterialIndices;
    Array<Material> mMaterials;
};

}

#endif
