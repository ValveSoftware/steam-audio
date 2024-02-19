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

#include "static_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// StaticMesh
// --------------------------------------------------------------------------------------------------------------------

StaticMesh::StaticMesh(int numVertices,
                       int numTriangles,
                       int numMaterials,
                       const Vector3f* vertices,
                       const Triangle* triangles,
                       const int* materialIndices,
                       const Material* materials)
    : mMesh(numVertices, numTriangles, vertices, triangles)
    , mBVH(mMesh)
    , mMaterialIndices(numTriangles)
    , mMaterials(numMaterials)
{
    memcpy(mMaterialIndices.data(), materialIndices, numTriangles * sizeof(int));
    memcpy(mMaterials.data(), materials, numMaterials * sizeof(Material));
}

StaticMesh::StaticMesh(const Serialized::StaticMesh* serializedObject)
    : mMesh(serializedObject->mesh())
    , mBVH(mMesh)
    , mMaterialIndices(mMesh.numTriangles())
{
    assert(serializedObject);
    assert(serializedObject->mesh());
    assert(serializedObject->material_indices() && serializedObject->material_indices()->Length() > 0);
    assert(serializedObject->materials() && serializedObject->materials()->Length() > 0);

    memcpy(mMaterialIndices.data(), serializedObject->material_indices()->data(), mMesh.numTriangles() * sizeof(int));

    auto numMaterials = serializedObject->materials()->Length();

    mMaterials.resize(numMaterials);
    memcpy(mMaterials.data(), serializedObject->materials()->data(), numMaterials * sizeof(Material));
}

StaticMesh::StaticMesh(SerializedObject& serializedObject)
    : StaticMesh(Serialized::GetStaticMesh(serializedObject.data()))
{}

flatbuffers::Offset<Serialized::StaticMesh> StaticMesh::serialize(SerializedObject& serializedObject) const
{
    auto& fbb = serializedObject.fbb();

    auto meshOffset = mMesh.serialize(serializedObject);

    auto materialIndicesOffset = fbb.CreateVector(mMaterialIndices.data(), mMesh.numTriangles());

    auto materialsOffset = fbb.CreateVectorOfStructs(reinterpret_cast<const Serialized::Material*>(mMaterials.data()), numMaterials());

    return Serialized::CreateStaticMesh(fbb, meshOffset, materialIndicesOffset, materialsOffset);
}

void StaticMesh::serializeAsRoot(SerializedObject& serializedObject) const
{
    serializedObject.fbb().Finish(serialize(serializedObject));
    serializedObject.commit();
}

Hit StaticMesh::closestHit(const Ray& ray,
                           float minDistance,
                           float maxDistance) const
{
    auto hit = mBVH.intersect(ray, mMesh, minDistance, maxDistance);

    if (hit.isValid())
    {
        hit.normal = mMesh.normal(hit.triangleIndex);
        hit.materialIndex = mMaterialIndices[hit.triangleIndex];
        hit.material = &mMaterials[hit.materialIndex];
    }

    return hit;
}

bool StaticMesh::anyHit(const Ray& ray,
                        float minDistance,
                        float maxDistance) const
{
    return mBVH.isOccluded(ray, mMesh, minDistance, maxDistance);
}

bool StaticMesh::intersectsBox(const Box& box) const
{
    return mBVH.intersect(box, mMesh);
}

}
