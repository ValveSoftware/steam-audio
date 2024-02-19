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
#include "instanced_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeInstancedMesh
// --------------------------------------------------------------------------------------------------------------------

class EmbreeInstancedMesh : public IInstancedMesh
{
public:
    EmbreeInstancedMesh(shared_ptr<EmbreeScene> scene,
                        shared_ptr<EmbreeScene> subScene,
                        const Matrix4x4f& transform);

    virtual ~EmbreeInstancedMesh() override;

    virtual int numVertices() const override
    {
        return mNumVertices;
    }

    virtual int numTriangles() const override
    {
        return mNumTriangles;
    }

    const EmbreeScene& subScene() const
    {
        return *mSubScene;
    }

    const Matrix4x4f& transform() const
    {
        return mTransform;
    }

    unsigned int instanceIndex() const
    {
        return mInstanceIndex;
    }

    virtual void updateTransform(const IScene& scene,
                                 const Matrix4x4f& transform) override;

    virtual void commit(const IScene& scene) override;

    // Returns true if the transform has changed since the previous call to commit().
    virtual bool hasChanged() const override;

    void enable(const EmbreeScene& scene);

    void disable(const EmbreeScene& scene);

private:
    std::weak_ptr<EmbreeScene> mScene;
    shared_ptr<EmbreeScene> mSubScene;
    int mNumVertices;
    int mNumTriangles;
    unsigned int mInstanceIndex;
    Matrix4x4f mTransform;

    // Flag indicating whether this instanced mesh has changed since the last call to commit().
    bool mHasChanged;
};

}

#endif
