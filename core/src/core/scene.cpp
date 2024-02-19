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

#include "scene.h"

#if defined(IPL_OS_WINDOWS)
#include <codecvt>
#endif

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IScene
// --------------------------------------------------------------------------------------------------------------------

bool IScene::isOccluded(const Vector3f& from,
                        const Vector3f& to) const
{
    return anyHit(Ray{ from, Vector3f::unitVector(to - from) }, 0.0f, (to - from).length());
}


// --------------------------------------------------------------------------------------------------------------------
// Scene
// --------------------------------------------------------------------------------------------------------------------

Scene::Scene()
    : mHasChanged(false)
    , mVersion(0)
{}

Scene::Scene(const Serialized::Scene* serializedObject)
    : mHasChanged(false)
    , mVersion(0)
{
    assert(serializedObject);
    assert(serializedObject->static_meshes() && serializedObject->static_meshes()->Length() > 0);

    auto numObjects = serializedObject->static_meshes()->Length();

    for (auto i = 0u; i < numObjects; ++i)
    {
        auto staticMesh = ipl::make_shared<StaticMesh>(serializedObject->static_meshes()->Get(i));
        mStaticMeshes[1].push_back(std::static_pointer_cast<IStaticMesh>(staticMesh));
    }

    mStaticMeshes[0] = mStaticMeshes[1];
}

Scene::Scene(SerializedObject& serializedObject)
    : Scene(Serialized::GetScene(serializedObject.data()))
{}

shared_ptr<IStaticMesh> Scene::createStaticMesh(int numVertices,
                                                int numTriangles,
                                                int numMaterials,
                                                const Vector3f* vertices,
                                                const Triangle* triangles,
                                                const int* materialIndices,
                                                const Material* materials)
{
    auto staticMesh = ipl::make_shared<StaticMesh>(numVertices, numTriangles, numMaterials, vertices, triangles,
                                                   materialIndices, materials);

    return std::static_pointer_cast<IStaticMesh>(staticMesh);
}

shared_ptr<IStaticMesh> Scene::createStaticMesh(SerializedObject& serializedObject)
{
    auto staticMesh = ipl::make_shared<StaticMesh>(serializedObject);
    return std::static_pointer_cast<IStaticMesh>(staticMesh);
}

shared_ptr<IInstancedMesh> Scene::createInstancedMesh(shared_ptr<IScene> subScene,
                                                      const Matrix4x4f& transform)
{
    auto instancedMesh = ipl::make_shared<InstancedMesh>(std::static_pointer_cast<Scene>(subScene), transform);
    return std::static_pointer_cast<IInstancedMesh>(instancedMesh);
}

void Scene::addStaticMesh(shared_ptr<IStaticMesh> staticMesh)
{
    mStaticMeshes[1].push_back(staticMesh);

    mHasChanged = true;
}

void Scene::removeStaticMesh(shared_ptr<IStaticMesh> staticMesh)
{
    mStaticMeshes[1].remove(staticMesh);

    mHasChanged = true;
}

void Scene::addInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh)
{
    mInstancedMeshes[1].push_back(instancedMesh);

    mHasChanged = true;
}

void Scene::removeInstancedMesh(shared_ptr<IInstancedMesh> instancedMesh)
{
    mInstancedMeshes[1].remove(instancedMesh);

    mHasChanged = true;
}

void Scene::commit()
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

    for (const auto& instancedMesh : mInstancedMeshes[0])
    {
        instancedMesh->commit(*this);
    }

    // The scene will be considered unchanged until something is changed subsequently.
    mHasChanged = false;
}

uint32_t Scene::version() const
{
    return mVersion;
}

Hit Scene::closestHit(const Ray& ray,
                      float minDistance,
                      float maxDistance) const
{
    Hit hit;

    // We sequentially calculate the closest hit of the ray with each scene object,
    // recording the overall closest hit in the scene. If there are many objects
    // in the scene, it would be better to use some sort of acceleration
    // structure.
    for (const auto& staticMesh : mStaticMeshes[0])
    {
        const auto phononStaticMesh = static_cast<const StaticMesh*>(staticMesh.get());
        auto objectHit = phononStaticMesh->closestHit(ray, minDistance, maxDistance);
        if (objectHit.distance < hit.distance)
        {
            hit = objectHit;
        }
    }

    for (const auto& instancedMesh : mInstancedMeshes[0])
    {
        const auto phononInstancedMesh = static_cast<const InstancedMesh*>(instancedMesh.get());
        auto objectHit = phononInstancedMesh->closestHit(ray, minDistance, maxDistance);
        if (objectHit.distance < hit.distance)
        {
            hit = objectHit;
        }
    }

    return hit;
}

bool Scene::anyHit(const Ray& ray,
                   float minDistance,
                   float maxDistance) const
{
    for (const auto& staticMesh : mStaticMeshes[0])
    {
        const auto phononStaticMesh = static_cast<const StaticMesh*>(staticMesh.get());
        if (phononStaticMesh->anyHit(ray, minDistance, maxDistance))
            return true;
    }

    for (const auto& instancedMesh : mInstancedMeshes[0])
    {
        const auto phononInstancedMesh = static_cast<const InstancedMesh*>(instancedMesh.get());
        if (phononInstancedMesh->anyHit(ray, minDistance, maxDistance))
            return true;
    }

    return false;
}

void Scene::closestHits(int numRays,
                        const Ray* rays,
                        const float* minDistances,
                        const float* maxDistances,
                        Hit* hits) const
{
    for (auto i = 0; i < numRays; ++i)
    {
        hits[i] = closestHit(rays[i], minDistances[i], maxDistances[i]);
    }
}

void Scene::anyHits(int numRays,
                    const Ray* rays,
                    const float* minDistances,
                    const float* maxDistances,
                    bool* occluded) const
{
    for (auto i = 0; i < numRays; ++i)
    {
        occluded[i] = (maxDistances[i] >= 0.0f) ? anyHit(rays[i], minDistances[i], maxDistances[i]) : true;
    }
}

bool Scene::intersectsBox(const Box& box) const
{
    for (const auto& staticMesh : mStaticMeshes[0])
    {
        if (static_cast<const StaticMesh*>(staticMesh.get())->intersectsBox(box))
            return true;
    }

    return false;
}

flatbuffers::Offset<Serialized::Scene> Scene::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    vector<flatbuffers::Offset<Serialized::StaticMesh>> staticMeshOffsets;
    staticMeshOffsets.reserve(numStaticMeshes());
    for (const auto& staticMesh : mStaticMeshes[0])
    {
        staticMeshOffsets.push_back(static_cast<StaticMesh*>(staticMesh.get())->serialize(serializedObject));
    }
    auto staticMeshesOffset = fbb.CreateVector(staticMeshOffsets.data(), numStaticMeshes());

    return Serialized::CreateScene(fbb, staticMeshesOffset);
}

void Scene::serializeAsRoot(SerializedObject& serializedObject) const
{
    serializedObject.fbb().Finish(serialize(serializedObject));
    serializedObject.commit();
}

void Scene::dumpObj(const string& fileName) const
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

    auto numVerticesDumped = 0;
    auto numTrianglesDumped = 0;
    auto numMaterialsDumped = 0;

    for (const auto& staticMesh : mStaticMeshes[0])
    {
        const auto& _staticMesh = *static_cast<const StaticMesh*>(staticMesh.get());

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
        for (auto i = 0; i < _staticMesh.numMaterials(); ++i)
        {
            const auto materials = _staticMesh.materials();

            float diffuseReflectivity[3] = {};
            float specularReflectivity[3] = {};
            float transmission[3] = {};

            for (auto j = 0; j < 3; ++j)
            {
                diffuseReflectivity[j] = (1.0f - materials[i].absorption[j]) * materials[i].scattering;
                specularReflectivity[j] = (1.0f - materials[i].absorption[j]) * (1.0f - materials[i].scattering);
                transmission[j] = materials[i].transmission[j];
            }

            fprintf(mtlFile, "newmtl material_%d\n", numMaterialsDumped + i);
            fprintf(mtlFile, "Kd %f %f %f\n", diffuseReflectivity[0], diffuseReflectivity[1], diffuseReflectivity[2]);
            fprintf(mtlFile, "Ks %f %f %f\n", specularReflectivity[0], specularReflectivity[1], specularReflectivity[2]);
            fprintf(mtlFile, "Tf %f %f %f\n\n", transmission[0], transmission[1], transmission[2]);
        }

        for (auto i = 0; i < _staticMesh.numVertices(); ++i)
        {
            const auto& vertex = _staticMesh.mesh().vertex(i);
            fprintf(objFile, "v %f %f %f\n", vertex.x(), vertex.y(), vertex.z());
        }

        auto previousMaterialIndex = -1;
        for (auto i = 0; i < _staticMesh.numTriangles(); ++i)
        {
            auto materialIndex = _staticMesh.materialIndices()[i];
            if (materialIndex != previousMaterialIndex)
            {
                fprintf(objFile, "usemtl material_%d\n", numMaterialsDumped + materialIndex);
                previousMaterialIndex = materialIndex;
            }

            const auto& triangle = _staticMesh.mesh().triangle(i);

            fprintf(objFile, "f ");
            for (auto j = 0; j < 3; ++j)
            {
                const auto index = triangle.indices[j];
                fprintf(objFile, "%d ", numVerticesDumped + index + 1);
            }
            fprintf(objFile, "\n");
        }

        numVerticesDumped += _staticMesh.numVertices();
        numTrianglesDumped += _staticMesh.numTriangles();
        numMaterialsDumped += _staticMesh.numMaterials();
    }

    fclose(mtlFile);
    fclose(objFile);
}

}
