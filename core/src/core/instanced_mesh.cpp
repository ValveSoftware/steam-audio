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

#include "instanced_mesh.h"
#include "scene.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// InstancedMesh
// --------------------------------------------------------------------------------------------------------------------

InstancedMesh::InstancedMesh(shared_ptr<Scene> subScene,
                             const Matrix4x4f& transform)
    : mSubScene(std::static_pointer_cast<Scene>(subScene))
    , mTransform(transform)
    , mNumVertices(0)
    , mNumTriangles(0)
    , mHasChanged(false)
{
    for (const auto& mesh : mSubScene->staticMeshes())
    {
        auto staticMesh = static_cast<const StaticMesh*>(mesh.get());
        mNumVertices += staticMesh->numVertices();
        mNumTriangles += staticMesh->numTriangles();
    }

    mTransform = mTransform.transposedCopy();
    inverse(mTransform, mInverseTransform);
}

void InstancedMesh::updateTransform(const IScene& scene,
                                    const Matrix4x4f& transform)
{
    auto transpose = transform.transposedCopy();

    // If the elements of the transform matrix have changed, consider this instanced mesh to have changed since the
    // last call to commit().
    if (memcmp(transpose.elements, mTransform.elements, 16 * sizeof(float)) != 0)
    {
        mHasChanged = true;
    }

    mTransform = transpose;
    inverse(mTransform, mInverseTransform);
}

void InstancedMesh::commit(const IScene& scene)
{
    mSubScene->commit();

    // After calling commit(), this instanced mesh will be considered unchanged until a subsequent call to
    // updateTransform() changes the transform matrix.
    mHasChanged = false;
}

bool InstancedMesh::hasChanged() const
{
    return mHasChanged;
}

Hit InstancedMesh::closestHit(const Ray& ray,
                              float minDistance,
                              float maxDistance) const
{
    auto transformedRay = inverseTransformRay(ray, minDistance, maxDistance);
    auto hit = mSubScene->closestHit(transformedRay, minDistance, maxDistance);
    return transformHit(hit, transformedRay);
}

bool InstancedMesh::anyHit(const Ray& ray,
                           float minDistance,
                           float maxDistance) const
{
    auto transformedRay = inverseTransformRay(ray, minDistance, maxDistance);
    return mSubScene->anyHit(transformedRay, minDistance, maxDistance);
}

Ray InstancedMesh::inverseTransformRay(const Ray& ray,
                                       float& minDistance,
                                       float& maxDistance) const
{
    auto origin = mInverseTransform * Vector4f(ray.origin);

    auto start = mInverseTransform * Vector4f(ray.pointAtDistance(minDistance));
    minDistance = (start - origin).length();

    if (maxDistance < std::numeric_limits<float>::infinity())
    {
        auto end = mInverseTransform * Vector4f(ray.pointAtDistance(maxDistance));
        maxDistance = (end - origin).length();
    }

    auto p = mInverseTransform * Vector4f(ray.pointAtDistance(1.0f));
    auto direction = Vector3f::unitVector(Vector3f(p[0], p[1], p[2]) - Vector3f(origin[0], origin[1], origin[2]));

    Ray transformedRay;
    transformedRay.origin = Vector3f(origin[0], origin[1], origin[2]);
    transformedRay.direction = direction;

    return transformedRay;
}

Hit InstancedMesh::transformHit(const Hit& hit,
                                const Ray& ray) const
{
    auto transformedHit = hit;

    if (hit.distance < std::numeric_limits<float>::infinity())
    {
        auto origin = mTransform * Vector4f(ray.origin);
        auto hitPoint = mTransform * Vector4f(ray.pointAtDistance(hit.distance));
        transformedHit.distance = (hitPoint - origin).length();
    }

    auto normal = Vector4f(hit.normal.x(), hit.normal.y(), hit.normal.z(), 0.0f);
    auto transformedNormal = mInverseTransform.transposedCopy() * normal;
    transformedHit.normal = Vector3f::unitVector(Vector3f(transformedNormal[0], transformedNormal[1], transformedNormal[2]));

    return transformedHit;
}

}
