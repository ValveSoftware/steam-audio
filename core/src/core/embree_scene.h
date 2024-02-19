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

#include "embree_device.h"
#include "scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeScene
// --------------------------------------------------------------------------------------------------------------------

// An IScene implementation that uses Embree as its ray tracer backend.
class EmbreeScene : public IScene
{
public:
    EmbreeScene(shared_ptr<EmbreeDevice> embree);

    EmbreeScene(shared_ptr<EmbreeDevice> embree,
                const Serialized::Scene* serializedObject);

    EmbreeScene(shared_ptr<EmbreeDevice> embree,
                SerializedObject& serializedObject);

    virtual ~EmbreeScene() override;

    virtual int numStaticMeshes() const override
    {
        return static_cast<int>(mStaticMeshes[1].size());
    }

    virtual int numInstancedMeshes() const override
    {
        return static_cast<int>(mInstancedMeshes[1].size());
    }

    RTCScene scene() const
    {
        return mScene;
    }

    const list<shared_ptr<IStaticMesh>>& staticMeshes() const
    {
        return mStaticMeshes[1];
    }

    const list<shared_ptr<IInstancedMesh>>& instancedMeshes() const
    {
        return mInstancedMeshes[1];
    }

    const Material* const* materialsForGeometry() const
    {
        return mMaterialsForGeometry.data();
    }

    const int* const* materialIndicesForGeometry() const
    {
        return mMaterialIndicesForGeometry.data();
    }

    virtual shared_ptr<IStaticMesh> createStaticMesh(int numVertices,
                                                     int numTriangles,
                                                     int numMaterials,
                                                     const Vector3f* vertices,
                                                     const Triangle* triangles,
                                                     const int* materialIndices,
                                                     const Material* materials) override;

    virtual shared_ptr<IStaticMesh> createStaticMesh(SerializedObject& serializedObject) override;

    virtual shared_ptr<IInstancedMesh> createInstancedMesh(shared_ptr<IScene> subScene,
                                                           const Matrix4x4f& transform) override;

    virtual void addStaticMesh(shared_ptr<IStaticMesh> staticMesh) override;

    virtual void removeStaticMesh(shared_ptr<IStaticMesh> staticMesh) override;

    virtual void addInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh) override;

    virtual void removeInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh) override;

    virtual void commit() override;

    // Returns the change version of the scene. Every time commit() is called after changing the scene (e.g., by adding
    // or removing a static or instanced mesh, or by updating the transform of an instanced mesh), the version number
    // is incremented.
    virtual uint32_t version() const override;

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

    virtual void dumpObj(const string& fileName) const override;

private:
    shared_ptr<EmbreeDevice> mEmbree;
    RTCScene mScene;
    list<shared_ptr<IStaticMesh>> mStaticMeshes[2];
    list<shared_ptr<IInstancedMesh>> mInstancedMeshes[2];
    Array<const Material*> mMaterialsForGeometry;
    Array<const int*> mMaterialIndicesForGeometry;

    // Flag indicating whether the scene has changed in some way since the previous call to commit().
    bool mHasChanged;

    // The change version of the scene.
    uint32_t mVersion;

    void initialize();
};

}

#endif
