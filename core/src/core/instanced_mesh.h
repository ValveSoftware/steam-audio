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

#include "hit.h"
#include "matrix.h"
#include "ray.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IInstancedMesh
// --------------------------------------------------------------------------------------------------------------------

class IScene;
class Scene;

class IInstancedMesh
{
public:
    virtual ~IInstancedMesh()
    {}

    virtual int numVertices() const = 0;

    virtual int numTriangles() const = 0;

    virtual void updateTransform(const IScene& scene,
                                 const Matrix4x4f& transform) = 0;

    virtual void commit(const IScene& scene) = 0;

    virtual void setObjectIndex(int newObjectIndex) = 0;

    virtual int getObjectIndex() const = 0;

    // Returns true if the transform has changed since the previous call to commit().
    virtual bool hasChanged() const = 0;
};


// --------------------------------------------------------------------------------------------------------------------
// InstancedMesh
// --------------------------------------------------------------------------------------------------------------------

class InstancedMesh : public IInstancedMesh
{
public:
    InstancedMesh(shared_ptr<Scene> subScene,
                  const Matrix4x4f& transform);

    virtual int numVertices() const override
    {
        return mNumVertices;
    }

    virtual int numTriangles() const override
    {
        return mNumTriangles;
    }

    const Scene& subScene() const
    {
        return *mSubScene;
    }

    const Matrix4x4f& transform() const
    {
        return mTransform;
    }

    virtual void updateTransform(const IScene& scene,
                                 const Matrix4x4f& transform) override;

    virtual void commit(const IScene& scene) override;

    virtual void setObjectIndex(int newObjectIndex) override { mObjectIndex = newObjectIndex; }

    virtual int getObjectIndex() const override { return mObjectIndex; }

    // Returns true if the transform has changed since the previous call to commit().
    virtual bool hasChanged() const override;

    Hit closestHit(const Ray& ray,
                   float minDistance,
                   float maxDistance) const;

    bool anyHit(const Ray& ray,
                float minDistance,
                float maxDistance) const;

private:
    shared_ptr<Scene> mSubScene;
    Matrix4x4f mTransform;
    Matrix4x4f mInverseTransform;
    int mNumVertices;
    int mNumTriangles;
    int mObjectIndex;

    // Flag indicating whether this instanced mesh has changed since the last call to commit().
    bool mHasChanged;

    Ray inverseTransformRay(const Ray& ray,
                            float& minDistance,
                            float& maxDistance) const;

    Hit transformHit(const Hit& hit,
                     const Ray& ray) const;
};

}
