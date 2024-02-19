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

#include "context.h"
#include "instanced_mesh.h"
#include "material.h"
#include "static_mesh.h"

#include "scene.fbs.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IScene
// --------------------------------------------------------------------------------------------------------------------

// A 3D scene, comprised of multiple kinds of SceneObjects. Objects can be added and removed from the scene at any
// time. Objects can also be defined as instances of one another. This class also allows rays to be traced through
// the scene.
class IScene : public std::enable_shared_from_this<IScene>
{
public:
    virtual ~IScene()
    {}

    virtual int numStaticMeshes() const = 0;

    virtual int numInstancedMeshes() const = 0;

    virtual shared_ptr<IStaticMesh> createStaticMesh(int numVertices,
                                                     int numTriangles,
                                                     int numMaterials,
                                                     const Vector3f* vertices,
                                                     const Triangle* triangles,
                                                     const int* materialIndices,
                                                     const Material* materials) = 0;

    virtual shared_ptr<IStaticMesh> createStaticMesh(SerializedObject& serializedObject) = 0;

    virtual shared_ptr<IInstancedMesh> createInstancedMesh(shared_ptr<IScene> subScene,
                                                           const Matrix4x4f& transform) = 0;

    virtual void addStaticMesh(shared_ptr<IStaticMesh> staticMesh) = 0;

    virtual void removeStaticMesh(shared_ptr<IStaticMesh> staticMesh) = 0;

    virtual void addInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh) = 0;

    virtual void removeInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh) = 0;

    virtual void commit() = 0;

    // Returns the change version of the scene. Every time commit() is called after changing the scene (e.g., by adding
    // or removing a static or instanced mesh, or by updating the transform of an instanced mesh), the version number
    // is incremented.
    virtual uint32_t version() const = 0;

    virtual Hit closestHit(const Ray& ray,
                           float minDistance,
                           float maxDistance) const = 0;

    virtual bool anyHit(const Ray& ray,
                        float minDistance,
                        float maxDistance) const = 0;

    virtual void closestHits(int numRays,
                             const Ray* rays,
                             const float* minDistances,
                             const float* maxDistances,
                             Hit* hits) const = 0;

    virtual void anyHits(int numRays,
                         const Ray* rays,
                         const float* minDistances,
                         const float* maxDistances,
                         bool* occluded) const = 0;

    virtual void dumpObj(const string& fileName) const = 0;

    bool isOccluded(const Vector3f& from,
                    const Vector3f& to) const;
};


// --------------------------------------------------------------------------------------------------------------------
// Scene
// --------------------------------------------------------------------------------------------------------------------

class Scene : public IScene
{
public:
    Scene();

    Scene(const Serialized::Scene* serializedObject);

    Scene(SerializedObject& serializedObject);

    virtual int numStaticMeshes() const override
    {
        return static_cast<int>(mStaticMeshes[0].size());
    }

    virtual int numInstancedMeshes() const override
    {
        return static_cast<int>(mInstancedMeshes[0].size());
    }

    const list<shared_ptr<IStaticMesh>>& staticMeshes() const
    {
        return mStaticMeshes[0];
    }

    const list<shared_ptr<IInstancedMesh>>& instancedMeshes() const
    {
        return mInstancedMeshes[1];
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

    bool intersectsBox(const Box& box) const;

    flatbuffers::Offset<Serialized::Scene> serialize(SerializedObject& serializedObject) const;

    void serializeAsRoot(SerializedObject& serializedObject) const;

private:
    list<shared_ptr<IStaticMesh>> mStaticMeshes[2];
    list<shared_ptr<IInstancedMesh>> mInstancedMeshes[2];

    // Flag indicating whether the scene has changed in some way since the previous call to commit().
    bool mHasChanged;

    // The change version of the scene.
    uint32_t mVersion;
};

}
