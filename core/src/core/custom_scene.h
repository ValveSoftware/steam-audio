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

#include "scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// CustomScene
// --------------------------------------------------------------------------------------------------------------------

typedef void (IPL_CALLBACK *ClosestHitCallback)(const Ray* ray,
                                                float minDistance,
                                                float maxDistance,
                                                Hit* hit,
                                                void* userData);

typedef void (IPL_CALLBACK *AnyHitCallback)(const Ray* ray,
                                            float minDistance,
                                            float maxDistance,
                                            uint8_t* occluded,
                                            void* userData);

typedef void (IPL_CALLBACK *BatchedClosestHitCallback)(int numRays,
                                                       const Ray* rays,
                                                       const float* minDistances,
                                                       const float* maxDistances,
                                                       Hit* hits,
                                                       void* userData);

typedef void (IPL_CALLBACK *BatchedAnyHitCallback)(int numRays,
                                                   const Ray* rays,
                                                   const float* minDistances,
                                                   const float* maxDistances,
                                                   uint8_t* occluded,
                                                   void* userData);

// An IScene implementation that calls back into a user-specified custom ray tracer.
class CustomScene : public IScene
{
public:
    CustomScene(ClosestHitCallback closestHitCallback,
                AnyHitCallback anyHitCallback,
                BatchedClosestHitCallback batchedClosestHitCallback,
                BatchedAnyHitCallback batchedAnyHitCallback,
                void* userData);

    virtual int numStaticMeshes() const override
    {
        return 0;
    }

    virtual int numInstancedMeshes() const override
    {
        return 0;
    }

    virtual shared_ptr<IStaticMesh> createStaticMesh(int numVertices,
                                                     int numTriangles,
                                                     int numMaterials,
                                                     const Vector3f* vertices,
                                                     const Triangle* triangles,
                                                     const int* materialIndices,
                                                     const Material* materials) override
    {
        return nullptr;
    }

    virtual shared_ptr<IStaticMesh> createStaticMesh(SerializedObject& serializedObject) override
    {
        return nullptr;
    }

    virtual shared_ptr<IInstancedMesh> createInstancedMesh(shared_ptr<IScene> subScene,
                                                           const Matrix4x4f& transform) override
    {
        return nullptr;
    }

    virtual void addStaticMesh(shared_ptr<IStaticMesh> object) override
    {}

    virtual void removeStaticMesh(shared_ptr<IStaticMesh> object) override
    {}

    virtual void addInstancedMesh(shared_ptr<IInstancedMesh> object) override
    {}

    virtual void removeInstancedMesh(shared_ptr<IInstancedMesh> object) override
    {}

    virtual void commit() override
    {}

    // TODO: Implement this method.
    virtual uint32_t version() const override
    {
        return 0;
    }

    virtual Hit closestHit(const Ray& ray,
                           float minDistance,
                           float maxDistance) const override;

    virtual bool anyHit(const Ray& ray,
                        float minDistance,
                        float maxDistance) const override;

    virtual void closestHits(int numRays,
                             const Ray* rays,
                             const float* minDistances,
                             const float* maxDistances,
                             Hit* hits) const override;

    virtual void anyHits(int numRays,
                         const Ray* rays,
                         const float* minDistances,
                         const float* maxDistances,
                         bool* occluded) const override;

    virtual void dumpObj(const string& fileName) const override
    {}

private:
    ClosestHitCallback mClosestHitCallback;
    AnyHitCallback mAnyHitCallback;
    BatchedClosestHitCallback mBatchedClosestHitCallback;
    BatchedAnyHitCallback mBatchedAnyHitCallback;
    void* mUserData;
};

}
