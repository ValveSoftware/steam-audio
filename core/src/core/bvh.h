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

#include "box.h"
#include "float4.h"
#include "mesh.h"
#include "platform.h"
#include "ray.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// BVHNode
// --------------------------------------------------------------------------------------------------------------------

// A node in a BVH. All the information required to represent a node,
// including information and children, and BVH splitting planes, is
// compactly stored in an array of 32-byte Box objects. Each Box represents
// a node and its bounding box. The remaining information is encoded in the
// first 4 bytes of padding, as follows:
//
//  Leaf nodes:
//      30 bits     triangle index
//       2 bits     the constant value 3
//
//  Internal nodes:
//      30 bits     offset from the current node to its left child
//       2 bits     split axis (0 = x, 1 = y, 2 = z).
class BVHNode
{
public:
    bool isLeaf() const
    {
        return (getSplitAxis() == 3);
    }

    int32_t getSplitAxis() const
    {
        return (data() & 3);
    }

    int32_t getTriangleIndex() const
    {
        return (data() >> 2);
    }

    void setTriangleIndex(int32_t triangleIndex)
    {
        data() = (triangleIndex << 2) | 3;
    }

    void setInternalNodeData(int32_t childOffset,
                             int32_t splitAxis)
    {
        data() = (childOffset << 2) | splitAxis;
    }

    BVHNode& leftChild()
    {
        return this[data() >> 2];
    }

    BVHNode& rightChild()
    {
        return this[(data() >> 2) + 1];
    }

    Box& boundingBox()
    {
        return mBoundingBox;
    }

    const Box& boundingBox() const
    {
        return mBoundingBox;
    }

private:
    int32_t& data()
    {
        return reinterpret_cast<int32_t*>(&mBoundingBox.minCoordinates)[3];
    }

    const int32_t& data() const
    {
        return reinterpret_cast<const int32_t*>(&mBoundingBox.minCoordinates)[3];
    }

    Box mBoundingBox;
};


// --------------------------------------------------------------------------------------------------------------------
// GrowableBox
// --------------------------------------------------------------------------------------------------------------------

// Represents a Box that can be efficiently grown to contain other primitives, using SIMD instructions.
class GrowableBox
{
public:
    GrowableBox()
    {
        reset();
    }

    void reset()
    {
        mMinCoordinates = float4::set1(std::numeric_limits<float>::max());
        mMaxCoordinates = float4::set1(-std::numeric_limits<float>::max());
    }

    void growToContain(const Vector3f& point)
    {
        auto pointCoordinates = float4::load(point.elements);
        mMinCoordinates = float4::min(mMinCoordinates, pointCoordinates);
        mMaxCoordinates = float4::max(mMaxCoordinates, pointCoordinates);
    }

    void growToContain(const Mesh& mesh,
                       int triangleIndex)
    {
        growToContain(mesh.triangleVertex(triangleIndex, 0));
        growToContain(mesh.triangleVertex(triangleIndex, 1));
        growToContain(mesh.triangleVertex(triangleIndex, 2));
    }

    void growToContain(const GrowableBox& box)
    {
        mMinCoordinates = float4::min(mMinCoordinates, box.mMinCoordinates);
        mMaxCoordinates = float4::max(mMaxCoordinates, box.mMaxCoordinates);
    }

    void load(const Box& box)
    {
        mMinCoordinates = float4::load(box.minCoordinates.elements);
        mMaxCoordinates = float4::load(box.maxCoordinates.elements);
    }

    void store(Box& box) const
    {
        float4::store(box.minCoordinates.elements, mMinCoordinates);
        float4::store(box.maxCoordinates.elements, mMaxCoordinates);
    }

    float getSurfaceArea() const
    {
        auto extents = float4::sub(mMaxCoordinates, mMinCoordinates);

#if (defined(IPL_CPU_ARMV7) || defined(IPL_CPU_ARM64))
        alignas(Memory::kDefaultAlignment)float extentsArray[4];
        float4::store(extentsArray, extents);
        return 2.0f * (extentsArray[0] * extentsArray[1] + extentsArray[1] * extentsArray[2] + extentsArray[2] * extentsArray[0]);
#else
        // The box extents are stored in an SSE register as [dx dy dz ?]
        // We first shuffle this register and multiply the result, to
        // obtain [dx dy dz ?] * [dy dz dx ?] = [dxdy dydz dzdx ?].
        extents = float4::mul(extents, _mm_shuffle_ps(extents, extents, _MM_SHUFFLE(3, 0, 2, 1)));

        // Now we shuffle this product two times, and add the resulting three
        // terms together, and multiply the result by 2:
        // 2 * ([dxdy dydz dzdx ?] + [dydz dzdx dxdy ?] + [dzdx dxdy dydz ?])
        // = [A A A ?], where A is the surface area.
        extents = float4::add(extents, float4::add(_mm_shuffle_ps(extents, extents, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(extents, extents, _MM_SHUFFLE(3, 0, 2, 1))));
        extents = float4::mul(extents, float4::set1(2.0f));

        // Save the result.
        alignas(Memory::kDefaultAlignment) float surfaceArea[4];
        float4::store(surfaceArea, extents);
        return surfaceArea[0];
#endif
    }

private:
    float4_t mMinCoordinates;
    float4_t mMaxCoordinates;
};


// --------------------------------------------------------------------------------------------------------------------
// CentroidCoordinate
// --------------------------------------------------------------------------------------------------------------------

// Represents a single coordinate of a leaf node centroid.
struct CentroidCoordinate
{
    float coordinate;
    int32_t leafIndex;
};


// --------------------------------------------------------------------------------------------------------------------
// Split
// --------------------------------------------------------------------------------------------------------------------

// Represents a split of an array of leaf nodes into two sub-arrays.
struct Split
{
    int32_t index;
    int32_t axis;
};


// --------------------------------------------------------------------------------------------------------------------
// BVH
// --------------------------------------------------------------------------------------------------------------------

// A Bounding Volume Hierarchy (BVH), consisting of axis-aligned bounding boxes (AABBs).
class BVH
{
public:
    BVH(const Mesh& mesh,
        ProgressCallback progressCallback = nullptr,
        void* userData = nullptr);

    int32_t numNodes() const
    {
        return static_cast<int32_t>(mNodes.size(0));
    }

    BVHNode& node(int32_t index)
    {
        return mNodes[index];
    }

    const BVHNode& node(int32_t index) const
    {
        return mNodes[index];
    }

    // Calculates the first intersection between a ray and any triangle in the BVH.
    Hit intersect(const Ray& ray,
                  const Mesh& mesh,
                  float minDistance,
                  float maxDistance) const;

    // Checks whether a ray is occluded by any triangle in the BVH.
    bool isOccluded(const Ray& ray,
                    const Mesh& mesh,
                    float minDistance,
                    float maxDistance) const;

    // Checks whether the ray between two points is occluded by any
    // triangle in the BVH. This function does not apply any tolerances
    // at either end point, so if either start or end is close to a
    // surface (as is likely to occur for reflected or shadow rays), the
    // ray may intersect with the reflecting surface. Care must be taken in
    // such cases to add an appropriate tolerance to the start point.
    bool isOccluded(const Vector3f& start,
                    const Vector3f& end,
                    const Mesh& mesh) const;

    // Returns true if the given box contains any geometry.
    bool intersect(const Box& box,
                   const Mesh& mesh) const;

    // Returns true if the given boxes intersect.
    static bool boxIntersectsBox(const Box& box1,
                                 const Box& box2);

private:
    static const int kConstructionStackDepth = 128; // Maximum recursion depth during BVH construction.
    static const int kTraversalStackDepth = 128; // Maximum recursion depth during BVH traversal.

    Array<BVHNode> mNodes; // The nodes of the BVH.

    // Builds a BVH using the triangles in a Mesh.
    void build(const Mesh& mesh,
               ProgressCallback progressCallback,
               void* userData);

    // Calculates the best split between the triangles in an internal node.
    Split bestSplit(GrowableBox* leafNodes,
                    int32_t* leafIndices,
                    CentroidCoordinate* const* centroids,
                    float* surfaceAreas,
                    const Box& boundingBox,
                    int32_t startIndex,
                    int32_t endIndex);

    // Uses the object median split approach for splitting an internal node. This
    // is worse than an SAH split, but is useful in certain degenerate
    // cases. The set of leaves is split at the median leaf index: roughly half
    // of the leaves end up in the left child, the rest in the right child.
    Split medianSplit(GrowableBox* leafNodes,
                      int32_t* leafIndices,
                      CentroidCoordinate* const* centroids,
                      const Box& boundingBox,
                      int32_t startIndex,
                      int32_t endIndex);

    // Uses the Surface Area Heuristic (SAH) split approach for splitting
    // an internal node. Results in better ray tracing performance than
    // the median split approach, but does not work in certain degenerate
    // cases.
    Split sahSplit(GrowableBox* leafNodes,
                   int32_t* leafIndices,
                   CentroidCoordinate* const* centroids,
                   float* surfaceAreas,
                   const Box& boundingBox,
                   int32_t startIndex,
                   int32_t endIndex);

    // Evaluates the SAH cost function.
    float sahCost(float leftChildSurfaceArea,
                  int32_t numLeftChildren,
                  float rightChildSurfaceArea,
                  int32_t numRightChildren,
                  float parentSurfaceArea) const;

    // Returns true if the given triangle intersects the given box.
    static bool boxIntersectsTriangle(const Box& box,
                                      const Mesh& mesh,
                                      int32_t triangleIndex);
};

}
