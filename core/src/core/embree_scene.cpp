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

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#include "log.h"
#include "embree_device.h"
#include "embree_instanced_mesh.h"
#include "embree_scene.h"
#include "embree_static_mesh.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EmbreeScene
// --------------------------------------------------------------------------------------------------------------------

EmbreeScene::EmbreeScene(shared_ptr<EmbreeDevice> embree)
    : mEmbree(embree)
    , mHasChanged(false)
    , mVersion(0)
{
    initialize();
}

EmbreeScene::EmbreeScene(shared_ptr<EmbreeDevice> embree,
                         const Serialized::Scene* serializedObject)
    : mEmbree(embree)
    , mHasChanged(false)
    , mVersion(0)
{
    assert(serializedObject);
    assert(serializedObject->static_meshes() && serializedObject->static_meshes()->Length() > 0);

    initialize();

    auto numObjects = serializedObject->static_meshes()->Length();

    for (auto i = 0u; i < numObjects; ++i)
    {
        auto staticMesh = ipl::make_shared<EmbreeStaticMesh>(std::static_pointer_cast<EmbreeScene>(shared_from_this()), serializedObject->static_meshes()->Get(i));
        addStaticMesh(std::static_pointer_cast<IStaticMesh>(staticMesh));
    }

    commit();
}

EmbreeScene::EmbreeScene(shared_ptr<EmbreeDevice> embree,
                         SerializedObject& serializedObject)
    : EmbreeScene(embree, Serialized::GetScene(serializedObject.data()))
{}

void EmbreeScene::initialize()
{
    auto sceneFlags = RTC_SCENE_DYNAMIC | RTC_SCENE_HIGH_QUALITY | RTC_SCENE_INCOHERENT;
    auto algorithmFlags = RTC_INTERSECT1 | RTC_INTERSECT4 | RTC_INTERSECT8 | RTC_INTERSECT16 | RTC_INTERSECT_STREAM;

    mScene = rtcDeviceNewScene(mEmbree->device(), sceneFlags, algorithmFlags);
}

EmbreeScene::~EmbreeScene()
{
    rtcDeleteScene(mScene);
}

shared_ptr<IStaticMesh> EmbreeScene::createStaticMesh(int numVertices,
                                                      int numTriangles,
                                                      int numMaterials,
                                                      const Vector3f* vertices,
                                                      const Triangle* triangles,
                                                      const int* materialIndices,
                                                      const Material* materials)
{
    auto staticMesh = ipl::make_shared<EmbreeStaticMesh>(std::static_pointer_cast<EmbreeScene>(shared_from_this()), numVertices, numTriangles, numMaterials, vertices, triangles,
                                                    materialIndices, materials);

    return std::static_pointer_cast<IStaticMesh>(staticMesh);
}

shared_ptr<IStaticMesh> EmbreeScene::createStaticMesh(SerializedObject& serializedObject)
{
    auto staticMesh = ipl::make_shared<EmbreeStaticMesh>(std::static_pointer_cast<EmbreeScene>(shared_from_this()), serializedObject);
    return std::static_pointer_cast<IStaticMesh>(staticMesh);
}

shared_ptr<IInstancedMesh> EmbreeScene::createInstancedMesh(shared_ptr<IScene> subScene,
                                                            const Matrix4x4f& transform)
{
    auto embreeSubScene = std::static_pointer_cast<EmbreeScene>(subScene);
    auto instancedMesh = ipl::make_shared<EmbreeInstancedMesh>(std::static_pointer_cast<EmbreeScene>(shared_from_this()), embreeSubScene, transform);
    return std::static_pointer_cast<IInstancedMesh>(instancedMesh);
}

void EmbreeScene::addStaticMesh(shared_ptr<IStaticMesh> staticMesh)
{
    mStaticMeshes[1].push_back(staticMesh);
    reinterpret_cast<EmbreeStaticMesh*>(staticMesh.get())->enable(*this);

    mHasChanged = true;
}

void EmbreeScene::removeStaticMesh(shared_ptr<IStaticMesh> staticMesh)
{
    reinterpret_cast<EmbreeStaticMesh*>(staticMesh.get())->disable(*this);
    mStaticMeshes[1].remove(staticMesh);

    mHasChanged = true;
}

void EmbreeScene::addInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh)
{
    mInstancedMeshes[1].push_back(instancedMesh);
    reinterpret_cast<EmbreeInstancedMesh*>(instancedMesh.get())->enable(*this);

    mHasChanged = true;
}

void EmbreeScene::removeInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh)
{
    reinterpret_cast<EmbreeInstancedMesh*>(instancedMesh.get())->disable(*this);
    mInstancedMeshes[1].remove(instancedMesh);

    mHasChanged = true;
}

void EmbreeScene::commit()
{
    // If no static/instanced meshes have been added or removed since the last commit(), check to see if any
    // instanced meshes have had their transforms updated.
    if (!mHasChanged)
    {
        for (const auto& instancedMesh : mInstancedMeshes[0])
        {
            if (instancedMesh->hasChanged())
            {
                mHasChanged = true;
                break;
            }
        }
    }

    // If something changed in the scene, increment the version.
    if (mHasChanged)
    {
        mVersion++;
    }

    mStaticMeshes[0] = mStaticMeshes[1];
    mInstancedMeshes[0] = mInstancedMeshes[1];

    for (auto& instancedMesh : mInstancedMeshes[0])
    {
        instancedMesh->commit(*this);
    }

    rtcCommit(mScene);

    uint32_t maxID = 0;

    for (const auto& staticMesh : mStaticMeshes[0])
    {
        const auto* embreeStaticMesh = static_cast<const EmbreeStaticMesh*>(staticMesh.get());
        maxID = std::max(maxID, embreeStaticMesh->geometryIndex());
    }

    for (const auto& instancedMesh : mInstancedMeshes[0])
    {
        const auto* embreeInstancedMesh = static_cast<const EmbreeInstancedMesh*>(instancedMesh.get());
        maxID = std::max(maxID, embreeInstancedMesh->instanceIndex());
    }

    mMaterialsForGeometry.resize(maxID + 1);
    mMaterialIndicesForGeometry.resize(maxID + 1);

    for (const auto& staticMesh : mStaticMeshes[0])
    {
        const auto* embreeStaticMesh = reinterpret_cast<const EmbreeStaticMesh*>(staticMesh.get());

        auto index = embreeStaticMesh->geometryIndex();

        mMaterialsForGeometry[index] = embreeStaticMesh->materials();
        mMaterialIndicesForGeometry[index] = embreeStaticMesh->materialIndices();
    }

    for (const auto& instancedMesh : mInstancedMeshes[0])
    {
        const auto* embreeInstancedMesh = reinterpret_cast<const EmbreeInstancedMesh*>(instancedMesh.get());
        const auto* embreeSubScene = &embreeInstancedMesh->subScene();
        const auto* embreeStaticMesh = reinterpret_cast<const EmbreeStaticMesh*>(embreeSubScene->staticMeshes().front().get());

        auto index = embreeInstancedMesh->instanceIndex();

        mMaterialsForGeometry[index] = embreeStaticMesh->materials();
        mMaterialIndicesForGeometry[index] = embreeStaticMesh->materialIndices();
    }

    // The scene will be considered unchanged until something is changed subsequently.
    mHasChanged = false;
}

uint32_t EmbreeScene::version() const
{
    return mVersion;
}

Hit EmbreeScene::closestHit(const Ray& ray,
                            float minDistance,
                            float maxDistance) const
{
    PROFILE_FUNCTION();

    RTCRay embreeRay = { 0 };
    embreeRay.org[0] = ray.origin.x();
    embreeRay.org[1] = ray.origin.y();
    embreeRay.org[2] = ray.origin.z();
    embreeRay.dir[0] = ray.direction.x();
    embreeRay.dir[1] = ray.direction.y();
    embreeRay.dir[2] = ray.direction.z();
    embreeRay.tnear = minDistance;
    embreeRay.tfar = maxDistance;
    embreeRay.mask = 0xffffffff;
    embreeRay.geomID = RTC_INVALID_GEOMETRY_ID;
    embreeRay.primID = RTC_INVALID_GEOMETRY_ID;
    embreeRay.instID = RTC_INVALID_GEOMETRY_ID;

    rtcIntersect(mScene, embreeRay);

    Hit hit;
    if (embreeRay.geomID != RTC_INVALID_GEOMETRY_ID)
    {
        auto geomID = (embreeRay.instID == RTC_INVALID_GEOMETRY_ID) ? embreeRay.geomID : embreeRay.instID;

        hit.distance = embreeRay.tfar;
        hit.normal = Vector3f::unitVector(Vector3f(embreeRay.Ng[0], embreeRay.Ng[1], embreeRay.Ng[2]));
        hit.material = &mMaterialsForGeometry[geomID][mMaterialIndicesForGeometry[geomID][embreeRay.primID]];
    }

    return hit;
}

bool EmbreeScene::anyHit(const Ray& ray,
                         float minDistance,
                         float maxDistance) const
{
    PROFILE_FUNCTION();

    RTCRay embreeRay = { 0 };
    embreeRay.org[0] = ray.origin.x();
    embreeRay.org[1] = ray.origin.y();
    embreeRay.org[2] = ray.origin.z();
    embreeRay.dir[0] = ray.direction.x();
    embreeRay.dir[1] = ray.direction.y();
    embreeRay.dir[2] = ray.direction.z();
    embreeRay.tnear = minDistance;
    embreeRay.tfar = maxDistance;
    embreeRay.mask = 0xffffffff;
    embreeRay.geomID = RTC_INVALID_GEOMETRY_ID;
    embreeRay.primID = RTC_INVALID_GEOMETRY_ID;
    embreeRay.instID = RTC_INVALID_GEOMETRY_ID;

    rtcOccluded(mScene, embreeRay);

    return (embreeRay.geomID != RTC_INVALID_GEOMETRY_ID);
}

void EmbreeScene::closestHits(int numRays,
                              const Ray* rays,
                              const float* minDistances,
                              const float* maxDistances,
                              Hit* hits) const
{
    PROFILE_FUNCTION();

    for (auto i = 0; i < numRays; ++i)
    {
        hits[i] = closestHit(rays[i], minDistances[i], maxDistances[i]);
    }
}

void EmbreeScene::anyHits(int numRays,
                          const Ray* rays,
                          const float* minDistances,
                          const float* maxDistances,
                          bool* occluded) const
{
    PROFILE_FUNCTION();

    for (auto i = 0; i < numRays; ++i)
    {
        occluded[i] = (maxDistances[i] >= 0.0f) ? anyHit(rays[i], minDistances[i], maxDistances[i]) : true;
    }
}

void EmbreeScene::dumpObj(const string& fileName) const
{
    auto utf8fopen = [](const string& fileName)
    {
#if defined(IPL_OS_WINDOWS)
        std::string utf8{ fileName.c_str() };
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring utf16{ converter.from_bytes(utf8) };
        return _wfopen(utf16.c_str(), L"w");
#else
        return fopen(fileName.c_str(), "w");
#endif
    };

    auto separatorPos = fileName.find_last_of("/\\");
    auto extensionPos = fileName.find_last_of(".");
    auto baseName = string{ fileName, separatorPos + 1, extensionPos - separatorPos - 1 };
    auto path = string{ fileName, 0, separatorPos + 1 };

    auto mtlFileName = path + baseName + ".mtl";
    auto mtlFile = utf8fopen(mtlFileName);
    fprintf(mtlFile, "# Generated by Steam Audio\n");

    auto objFile = utf8fopen(fileName);
    fprintf(objFile, "# Generated by Steam Audio\n");
    fprintf(objFile, "mtllib %s.mtl\n", baseName.c_str());

    vector<RTCScene> scenes;
    vector<EmbreeStaticMesh*> staticMeshes;
    vector<Matrix4x4f> transforms;

    for (const auto& staticMesh : mStaticMeshes[0])
    {
        scenes.push_back(mScene);
        staticMeshes.push_back((EmbreeStaticMesh*) staticMesh.get());
        transforms.push_back(Matrix4x4f::identityMatrix());
    }

    for (const auto& instancedMesh : mInstancedMeshes[0])
    {
        auto embreeInstancedMesh = (EmbreeInstancedMesh*) instancedMesh.get();
        scenes.push_back(embreeInstancedMesh->subScene().mScene);
        staticMeshes.push_back((EmbreeStaticMesh*) embreeInstancedMesh->subScene().mStaticMeshes[0].front().get());
        transforms.push_back(embreeInstancedMesh->transform());
    }

    auto vertexOffset = 1;
    auto materialOffset = 0;
    for (auto i = 0u; i < scenes.size(); ++i)
    {
        auto vertices = (float*) rtcMapBuffer(scenes[i], staticMeshes[i]->geometryIndex(), RTC_VERTEX_BUFFER);
        auto indices = (int32_t*) rtcMapBuffer(scenes[i], staticMeshes[i]->geometryIndex(), RTC_INDEX_BUFFER);

        // The OBJ file format does not use absorption and scattering coefficients; instead it uses diffuse
        // reflectivity (Kd) and specular reflectivity (Ks). They are defined by:
        //
        //  Kd = (1 - absorption) * scattering
        //  Ks = (1 - absorption) * (1 - scattering)
        //
        // To recover these values from the .mtl file, use the following equations:
        //
        //  scattering = Kd / (Kd + Ks)
        //  absorption = 1 - (Kd + Ks)
        //
        // The above equations hold for each band independently. Scattering coefficients will be equal for each
        // band. Transmission coefficients are stored as-is in the transmission filter (Tf) component of the
        // material.
        for (auto k = 0; k < staticMeshes[i]->numMaterials(); ++k)
        {
            const auto materials = staticMeshes[i]->materials();

            float diffuseReflectivity[3] = {};
            float specularReflectivity[3] = {};
            float transmission[3] = {};

            for (auto j = 0; j < 3; ++j)
            {
                diffuseReflectivity[j] = (1.0f - materials[k].absorption[j]) * materials[k].scattering;
                specularReflectivity[j] = (1.0f - materials[k].absorption[j]) * (1.0f - materials[k].scattering);
                transmission[j] = materials[k].transmission[j];
            }

            fprintf(mtlFile, "newmtl material_%d\n", materialOffset + k);
            fprintf(mtlFile, "Kd %f %f %f\n", diffuseReflectivity[0], diffuseReflectivity[1], diffuseReflectivity[2]);
            fprintf(mtlFile, "Ks %f %f %f\n", specularReflectivity[0], specularReflectivity[1], specularReflectivity[2]);
            fprintf(mtlFile, "Tf %f %f %f\n\n", transmission[0], transmission[1], transmission[2]);
        }

        for (auto j = 0; j < staticMeshes[i]->numVertices(); ++j)
        {
            auto vertex = Vector4f(vertices[4 * j + 0], vertices[4 * j + 1], vertices[4 * j + 2], 1.0f);
            auto transformedVertex = transforms[i].transposedCopy() * vertex;
            fprintf(objFile, "v %f %f %f\n", transformedVertex.elements[0], transformedVertex.elements[1],
                    transformedVertex.elements[2]);
        }

        auto previousMaterialIndex = -1;
        for (auto j = 0; j < staticMeshes[i]->numTriangles(); ++j)
        {
            auto materialIndex = staticMeshes[i]->materialIndices()[j];
            if (materialIndex != previousMaterialIndex)
            {
                fprintf(objFile, "usemtl material_%d\n", materialOffset + materialIndex);
                previousMaterialIndex = materialIndex;
            }

            fprintf(objFile, "f %d %d %d\n", vertexOffset + indices[3 * j + 0], vertexOffset + indices[3 * j + 1],
                    vertexOffset + indices[3 * j + 2]);
        }

        vertexOffset += staticMeshes[i]->numVertices();
        materialOffset += staticMeshes[i]->numMaterials();

        rtcUnmapBuffer(scenes[i], 0, RTC_INDEX_BUFFER);
        rtcUnmapBuffer(scenes[i], 0, RTC_VERTEX_BUFFER);
    }

    fclose(mtlFile);
    fclose(objFile);
}

}

#endif
