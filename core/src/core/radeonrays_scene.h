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

#if defined(IPL_USES_RADEONRAYS)

#include "radeonrays_device.h"
#include "scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysScene
// --------------------------------------------------------------------------------------------------------------------

class RadeonRaysScene : public IScene
{
public:
    RadeonRaysScene(shared_ptr<RadeonRaysDevice> radeonRays);

    RadeonRaysScene(shared_ptr<RadeonRaysDevice> radeonRays,
                    const Serialized::Scene* serializedObject);

    RadeonRaysScene(shared_ptr<RadeonRaysDevice> radeonRays,
                    SerializedObject& serializedObject);

    virtual int numStaticMeshes() const override
    {
        return static_cast<int>(mStaticMeshes.size());
    }

    virtual int numInstancedMeshes() const override
    {
        return 0;
    }

    virtual shared_ptr<IInstancedMesh> createInstancedMesh(shared_ptr<IScene> subScene,
                                                           const Matrix4x4f& transform) override
    {
        return nullptr;
    }

    virtual void addInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh) override
    {}

    virtual void removeInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh) override
    {}

    virtual Hit closestHit(const Ray& ray,
                           float minDistance,
                           float maxDistance) const override
    {
        if (mCPUScene)
            return mCPUScene->closestHit(ray, minDistance, maxDistance);
        else
            return Hit{};
    }

    virtual bool anyHit(const Ray& ray,
                        float minDistance,
                        float maxDistance) const override
    {
        if (mCPUScene)
            return mCPUScene->anyHit(ray, minDistance, maxDistance);
        else
            return false;
    }

    virtual void closestHits(int numRays,
                             const Ray* rays,
                             const float* minDistances,
                             const float* maxDistances,
                             Hit* hits) const override
    {
        if (mCPUScene)
            mCPUScene->closestHits(numRays, rays, minDistances, maxDistances, hits);
    }

    virtual void anyHits(int numRays,
                         const Ray* rays,
                         const float* minDistances,
                         const float* maxDistances,
                         bool* occluded) const override
    {
        if (mCPUScene)
            mCPUScene->anyHits(numRays, rays, minDistances, maxDistances, occluded);
    }

    virtual void dumpObj(const string& fileName) const override
    {}

    const list<shared_ptr<IStaticMesh>>& staticMeshes() const
    {
        return mStaticMeshes;
    }

    virtual shared_ptr<IStaticMesh> createStaticMesh(int numVertices,
                                                     int numTriangles,
                                                     int numMaterials,
                                                     const Vector3f* vertices,
                                                     const Triangle* triangles,
                                                     const int* materialIndices,
                                                     const Material* materials) override;

    virtual shared_ptr<IStaticMesh> createStaticMesh(SerializedObject& serializedObject) override;

    virtual void addStaticMesh(shared_ptr<IStaticMesh> staticMesh) override;

    virtual void removeStaticMesh(shared_ptr<IStaticMesh> staticMesh) override;

    virtual void commit() override;

    // TODO: Implement this method.
    virtual uint32_t version() const
    {
        return 0;
    }

private:
    shared_ptr<RadeonRaysDevice> mRadeonRays;
    list<shared_ptr<IStaticMesh>> mStaticMeshes;
    shared_ptr<Scene> mCPUScene;
};

}

#endif
