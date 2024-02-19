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

#include "bvh.h"

#include "stack.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ConstructionTask
// --------------------------------------------------------------------------------------------------------------------

// Represents a unit of work during BVH construction.
struct ConstructionTask
{
    int32_t outputNodeIndex;
    int32_t startIndex;
    int32_t endIndex;
    int32_t leftChildIndex;
};


// --------------------------------------------------------------------------------------------------------------------
// TraversalTask
// --------------------------------------------------------------------------------------------------------------------

// Represents a unit of work during BVH traversal.
struct TraversalTask
{
    int32_t nodeIndex;
    float tMin;
    float tMax;
};


// --------------------------------------------------------------------------------------------------------------------
// BVH
// --------------------------------------------------------------------------------------------------------------------

BVH::BVH(const Mesh& mesh,
         ProgressCallback progressCallback,
         void* userData)
    : mNodes(2 * mesh.numTriangles() - 1)
{
    build(mesh, progressCallback, userData);
}

void BVH::build(const Mesh& mesh,
                ProgressCallback progressCallback,
                void* userData)
{
    // The leafIndices array stores the indices of the mesh's triangles, in
    // left-to-right order as they appear in the final constructed BVH. When
    // construction begins, these are simply initialized in sorted order.
    // As construction proceeds, subarrays of this array will be permuted
    // based on how internal nodes are split.
    Array<int32_t> leafIndices(mesh.numTriangles());
    for (auto i = 0U; i < leafIndices.size(0); ++i)
    {
        leafIndices[i] = i;
    }

    // The leafNodes array stores the bounding boxes of each mesh triangle.
    Array<GrowableBox> leafNodes(mesh.numTriangles());
    for (auto i = 0; i < mesh.numTriangles(); ++i)
    {
        leafNodes[i].reset();
        leafNodes[i].growToContain(mesh, i);
    }

    // The leafBoxCenters array stores the centers of the bounding boxes of
    // each mesh triangle.
    Array<Vector3f> leafBoxCenters(mesh.numTriangles());
    for (auto i = 0; i < mesh.numTriangles(); ++i)
    {
        alignas(Memory::kDefaultAlignment) Box box;
        leafNodes[i].store(box);
        leafBoxCenters[i] = box.center();
    }

    // The centroids arrays are temporary storage used for sorting nodes by
    // centroid coordinates.
    Array<CentroidCoordinate, 2> centroids(3, mesh.numTriangles());

    // The surfaceAreas array is temporary storage used for calculating surface
    // areas of internal nodes.
    Array<float> surfaceAreas(mesh.numTriangles());

    // We begin by building the root node at index 0. It contains all the triangles in
    // the entire leafIndices array.
    Stack<ConstructionTask, kConstructionStackDepth> stack;
    ConstructionTask task = ConstructionTask{ 0, 0, static_cast<int32_t>(leafNodes.size(0)) - 1, 1 };

    // At each step of construction, we're processing a node containing
    // all the triangles in leafIndices[startIndex] to leafIndices[endIndex],
    // inclusive.
    while (true)
    {
        auto oneLeafLeft = (task.startIndex == task.endIndex);

        if (oneLeafLeft)
        {
            leafNodes[leafIndices[task.startIndex]].store(mNodes[task.outputNodeIndex].boundingBox());
            mNodes[task.outputNodeIndex].setTriangleIndex(leafIndices[task.startIndex]);

            if (stack.isEmpty())
                break;

            task = stack.pop();
        }
        else
        {
            // For internal nodes, we first construct a bounding box that
            // encloses all its triangles.
            GrowableBox boundingBox;
            for (auto i = task.startIndex; i <= task.endIndex; ++i)
            {
                boundingBox.growToContain(leafNodes[leafIndices[i]]);
            }

            boundingBox.store(mNodes[task.outputNodeIndex].boundingBox());

            // For each axis, centroids[axis][i] contains the coordinate of
            // the centroid of leaf node leafIndices[i].
            for (auto i = task.startIndex; i <= task.endIndex; ++i)
            {
                centroids[0][i].coordinate = leafBoxCenters[leafIndices[i]].x();
                centroids[1][i].coordinate = leafBoxCenters[leafIndices[i]].y();
                centroids[2][i].coordinate = leafBoxCenters[leafIndices[i]].z();
                centroids[0][i].leafIndex = leafIndices[i];
                centroids[1][i].leafIndex = leafIndices[i];
                centroids[2][i].leafIndex = leafIndices[i];
            }

            // Split the node into left and right children.
            auto split = bestSplit(leafNodes.data(), leafIndices.data(), centroids.data(), surfaceAreas.data(), mNodes[task.outputNodeIndex].boundingBox(), task.startIndex, task.endIndex);
            mNodes[task.outputNodeIndex].setInternalNodeData(task.leftChildIndex - task.outputNodeIndex, split.axis);

            // Push the right child onto the stack. Set the current task to the
            // left child, and continue.
            stack.push(ConstructionTask{ task.leftChildIndex + 1, task.startIndex + split.index, task.endIndex, task.leftChildIndex + 2 * split.index });
            task = ConstructionTask{ task.leftChildIndex, task.startIndex, task.startIndex + split.index - 1, task.leftChildIndex + 2 };
            continue;
        }
    }

    if (progressCallback)
    {
        progressCallback(1.0f, userData);
    }
}

Split BVH::bestSplit(GrowableBox* leafNodes,
                     int32_t* leafIndices,
                     CentroidCoordinate* const* centroids,
                     float* surfaceAreas,
                     const Box& boundingBox,
                     int32_t startIndex,
                     int32_t endIndex)
{
    // When finding the best split of an internal node, we first try the SAH
    // approach. If that fails (usually due to degenerate nodes), we use the
    // median split approach.
    auto split = sahSplit(leafNodes, leafIndices, centroids, surfaceAreas, boundingBox, startIndex, endIndex);
    if (split.axis == -1)
        split = medianSplit(leafNodes, leafIndices, centroids, boundingBox, startIndex, endIndex);

    return split;
}

Split BVH::medianSplit(GrowableBox* leafNodes,
                       int32_t* leafIndices,
                       CentroidCoordinate* const* centroids,
                       const Box& boundingBox,
                       int32_t startIndex,
                       int32_t endIndex)
{
    auto splitAxis = boundingBox.extents().indexOfMaxComponent();
    auto splitIndex = (endIndex - startIndex + 1) / 2;

    for (auto i = startIndex; i <= endIndex; ++i)
    {
        leafIndices[i] = centroids[splitAxis][i].leafIndex;
    }

    return Split{ splitIndex, static_cast<int32_t>(splitAxis) };
}

Split BVH::sahSplit(GrowableBox* leafNodes,
                    int32_t* leafIndices,
                    CentroidCoordinate* const* centroids,
                    float* surfaceAreas,
                    const Box& boundingBox,
                    int32_t startIndex,
                    int32_t endIndex)
{
    alignas(Memory::kDefaultAlignment) GrowableBox parentBox;
    parentBox.load(boundingBox);
    auto parentSurfaceArea = parentBox.getSurfaceArea();
    auto bestCost = std::numeric_limits<float>::max();
    auto split = Split{ -1, -1 };

    for (auto axis = 0; axis < 3; ++axis)
    {
        auto bestBalanceCost = std::numeric_limits<int>::max();

        auto centroidsForAxis = centroids[axis];

        // Sort the leaves by centroid coordinates.
        std::sort(&centroidsForAxis[startIndex], &centroidsForAxis[endIndex + 1], [](const CentroidCoordinate& a, const CentroidCoordinate& b)
        {
            return (a.coordinate < b.coordinate);
        });

        // Consider all possible splits, and evaluate the surface area of the
        // left child for each case.
        GrowableBox leftChildBox;
        leftChildBox.reset();
        for (auto index = startIndex; index < endIndex; ++index)
        {
            leftChildBox.growToContain(leafNodes[centroidsForAxis[index].leafIndex]);
            surfaceAreas[index] = leftChildBox.getSurfaceArea();
        }

        // Consider all possible splits, and evaluate the surface area of the
        // right child for each case. Also evaluate the SAH cost function and
        // find the best split.
        GrowableBox rightChildBox;
        rightChildBox.reset();
        for (auto index = endIndex, numLeftChildren = endIndex - startIndex, numRightChildren = 1; index > startIndex; --index, --numLeftChildren, ++numRightChildren)
        {
            rightChildBox.growToContain(leafNodes[centroidsForAxis[index].leafIndex]);
            auto cost = sahCost(surfaceAreas[index - 1], numLeftChildren, rightChildBox.getSurfaceArea(), numRightChildren, parentSurfaceArea);

            if (cost < bestCost)
            {
                bestCost = cost;
                split = Split{numLeftChildren, axis};
            }
            else if (cost == bestCost)
            {
                auto balanceCost = abs(numLeftChildren - ((endIndex - startIndex + 1) / 2));
                if (balanceCost < bestBalanceCost)
                {
                    bestBalanceCost = balanceCost;
                    split = Split{numLeftChildren, axis};
                }
            }
        }
    }

    // Permute the leafIndices of this node's subarray based on the
    // sorted order of leaves along the chosen axis.
    if (split.axis >= 0)
    {
        for (auto i = startIndex; i <= endIndex; ++i)
        {
            leafIndices[i] = centroids[split.axis][i].leafIndex;
        }
    }

    return split;
}

float BVH::sahCost(float leftChildSurfaceArea,
                   int32_t numLeftChildren,
                   float rightChildSurfaceArea,
                   int32_t numRightChildren,
                   float parentSurfaceArea) const
{
    return (leftChildSurfaceArea * numLeftChildren + rightChildSurfaceArea * numRightChildren) / parentSurfaceArea;
}

Hit BVH::intersect(const Ray& ray,
                   const Mesh& mesh,
                   float minDistance,
                   float maxDistance) const
{
    Hit hit;

    Vector3f reciprocalDirection(1.0f / ray.direction.x(), 1.0f / ray.direction.y(), 1.0f / ray.direction.z());
    if (ray.direction.x() == -0.0f) reciprocalDirection.x() = std::numeric_limits<float>::infinity();
    if (ray.direction.y() == -0.0f) reciprocalDirection.y() = std::numeric_limits<float>::infinity();
    if (ray.direction.z() == -0.0f) reciprocalDirection.z() = std::numeric_limits<float>::infinity();

    int directionSigns[3];
    directionSigns[0] = (ray.direction.x() >= 0) ? 1 : 0;
    directionSigns[1] = (ray.direction.y() >= 0) ? 1 : 0;
    directionSigns[2] = (ray.direction.z() >= 0) ? 1 : 0;

    // We start by checking for intersection with the root node.
    Stack<TraversalTask, kTraversalStackDepth> stack;
    TraversalTask task = { 0, minDistance, maxDistance };

    // In every step of the traversal, we test for intersection against
    // the current node.
    while (true)
    {
        const auto& node = mNodes[task.nodeIndex];

        // Check whether the ray passes through the bounding box of the
        // node.
        if (ray.intersect(node.boundingBox(), reciprocalDirection, directionSigns, task.tMin, task.tMax))
        {
            if (node.isLeaf())
            {
                // For leaf nodes, calculate the intersection of the ray
                // and the triangle. If this intersection lies on the ray,
                // and before the current closest hit, make this the
                // current closest hit.
                auto t = ray.intersect(mesh, node.getTriangleIndex());
                if (minDistance <= t && t < hit.distance)
                {
                    hit.distance = t;
                    hit.triangleIndex = node.getTriangleIndex();
                }
            }
            else
            {
                // Based on the ray signs, decide which of the two children
                // is the near child, and which is the far child. Push the
                // far child onto the stack, set the current node to the
                // near child, and continue.
                auto leftChildOffset = node.getTriangleIndex();
                auto splitAxis = node.getSplitAxis();
                stack.push(TraversalTask{ task.nodeIndex + leftChildOffset + directionSigns[splitAxis], task.tMin, task.tMax });
                task.nodeIndex += leftChildOffset + (directionSigns[splitAxis] ^ 1);
                continue;
            }
        }

        // If we've just processed a leaf, pop a new task off the stack.
        // If the stack is empty, stop.
        if (stack.isEmpty())
            break;

        task = stack.pop();
        task.tMax = std::min(task.tMax, hit.distance);
    }

    return hit;
}

bool BVH::isOccluded(const Ray& ray,
                     const Mesh& mesh,
                     float minDistance,
                     float maxDistance) const
{
    Vector3f reciprocalDirection(1.0f / ray.direction.x(), 1.0f / ray.direction.y(), 1.0f / ray.direction.z());
    if (ray.direction.x() == -0.0f) reciprocalDirection.x() = std::numeric_limits<float>::infinity();
    if (ray.direction.y() == -0.0f) reciprocalDirection.y() = std::numeric_limits<float>::infinity();
    if (ray.direction.z() == -0.0f) reciprocalDirection.z() = std::numeric_limits<float>::infinity();

    int directionSigns[3];
    directionSigns[0] = (ray.direction.x() >= 0) ? 1 : 0;
    directionSigns[1] = (ray.direction.y() >= 0) ? 1 : 0;
    directionSigns[2] = (ray.direction.z() >= 0) ? 1 : 0;

    // We start by checking for intersection with the root node.
    BVHNode* stack[kTraversalStackDepth];
    auto top = 0;
    auto node = const_cast<BVHNode*>(&mNodes[0]);

    // In every step of the traversal, we test for intersection against
    // the current node.
    while (true)
    {
        float tMin = minDistance;
        float tMax = maxDistance;

        // Check whether the ray passes through the bounding box of the
        // node.
        if (ray.intersect(node->boundingBox(), reciprocalDirection, directionSigns, tMin, tMax))
        {
            if (node->isLeaf())
            {
                // For leaf nodes, calculate the intersection of the ray
                // and the triangle. If this intersection lies on the ray,
                // the ray is occluded.
                auto t = ray.intersect(mesh, node->getTriangleIndex());
                if (minDistance <= t && t < maxDistance)
                    return true;
            }
            else
            {
                // Based on the ray signs, decide which of the two children
                // is the near child, and which is the far child. Push the
                // far child onto the stack, set the current node to the
                // near child, and continue.
                auto leftChildOffset = node->getTriangleIndex();
                auto splitAxis = node->getSplitAxis();
                stack[top++] = node + leftChildOffset + directionSigns[splitAxis];
                node += leftChildOffset + (directionSigns[splitAxis] ^ 1);
                continue;
            }
        }

        // If we've just processed a leaf, pop a new task off the stack.
        // If the stack is empty, stop.
        if (top <= 0)
            break;

        node = stack[--top];
    }

    return false;
}

bool BVH::isOccluded(const Vector3f& start,
                     const Vector3f& end,
                     const Mesh& mesh) const
{
    auto ray = Ray{ start, Vector3f::unitVector(end - start) };
    auto distance = (end - start).length();
    return isOccluded(ray, mesh, 0.0f, distance);
}

bool BVH::intersect(const Box& box,
                    const Mesh& mesh) const
{
    Stack<int32_t, kTraversalStackDepth> stack;

    auto nodeIndex = 0;

    while (true)
    {
        const auto& node = mNodes[nodeIndex];

        if (boxIntersectsBox(box, node.boundingBox()))
        {
            if (node.isLeaf())
            {
                if (boxIntersectsTriangle(box, mesh, node.getTriangleIndex()))
                    return true;
            }
            else
            {
                auto splitAxis = node.getSplitAxis();

                auto nearChildOffset = node.getTriangleIndex();
                auto farChildOffset = nearChildOffset + 1;
                if (box.minCoordinates[splitAxis] > node.boundingBox().minCoordinates[splitAxis])
                {
                    std::swap(nearChildOffset, farChildOffset);
                }

                stack.push(nodeIndex + farChildOffset);
                nodeIndex += nearChildOffset;
                continue;
            }
        }

        if (stack.isEmpty())
            break;

        nodeIndex = stack.pop();
    }

    return false;
}

bool BVH::boxIntersectsBox(const Box& box1,
                           const Box& box2)
{
    auto dx = std::max(0.0f, box2.minCoordinates.x() - box1.maxCoordinates.x()) + std::max(0.0f, box1.minCoordinates.x() - box2.maxCoordinates.x());
    auto dy = std::max(0.0f, box2.minCoordinates.y() - box1.maxCoordinates.y()) + std::max(0.0f, box1.minCoordinates.y() - box2.maxCoordinates.y());
    auto dz = std::max(0.0f, box2.minCoordinates.z() - box1.maxCoordinates.z()) + std::max(0.0f, box1.minCoordinates.z() - box2.maxCoordinates.z());

    return (dx == 0.0f && dy == 0.0f && dz == 0.0f);
}

bool BVH::boxIntersectsTriangle(const Box& box,
                                const Mesh& mesh,
                                int32_t triangleIndex)
{
    // if the bounding box of the triangle doesn't intersect the box, we shouldn't have reached this function

    // if the plane of the triangle doesn't intersect the box, stop

    auto v0 = mesh.triangleVertex(triangleIndex, 0);
    auto v1 = mesh.triangleVertex(triangleIndex, 1);
    auto v2 = mesh.triangleVertex(triangleIndex, 2);
    auto normal = mesh.normal(triangleIndex);
    auto extents = box.extents();

    auto criticalPointOffset = Vector3f(0.0f, 0.0f, 0.0f);
    if (normal.x() > 0.0f)
    {
        criticalPointOffset.x() = extents.x();
    }
    if (normal.y() > 0.0f)
    {
        criticalPointOffset.y() = extents.y();
    }
    if (normal.z() > 0.0f)
    {
        criticalPointOffset.z() = extents.z();
    }

    auto np = Vector3f::dot(normal, box.minCoordinates);
    auto d1 = Vector3f::dot(normal, criticalPointOffset - v0);
    auto d2 = Vector3f::dot(normal, (extents - criticalPointOffset) - v0);

    if ((np + d1) * (np + d2) > 0.0f)
        return false;

    // actual intersection tests

    // xy plane

    Vector3f e0 = v1 - v0;
    Vector3f e1 = v2 - v1;
    Vector3f e2 = v0 - v2;

    auto nxy0 = Vector2f(-e0.y(), e0.x());
    auto nxy1 = Vector2f(-e1.y(), e1.x());
    auto nxy2 = Vector2f(-e2.y(), e2.x());
    if (normal.z() < 0.0f)
    {
        nxy0 *= -1.0f;
        nxy1 *= -1.0f;
        nxy2 *= -1.0f;
    }

    auto dxy0 = -Vector2f::dot(nxy0, Vector2f(v0.x(), v0.y())) + std::max(0.0f, extents.x() * nxy0.x()) + std::max(0.0f, extents.y() * nxy0.y());
    auto dxy1 = -Vector2f::dot(nxy1, Vector2f(v1.x(), v1.y())) + std::max(0.0f, extents.x() * nxy1.x()) + std::max(0.0f, extents.y() * nxy1.y());
    auto dxy2 = -Vector2f::dot(nxy2, Vector2f(v2.x(), v2.y())) + std::max(0.0f, extents.x() * nxy2.x()) + std::max(0.0f, extents.y() * nxy2.y());

    if (Vector2f::dot(nxy0, Vector2f(box.minCoordinates.x(), box.minCoordinates.y())) + dxy0 < 0.0f ||
        Vector2f::dot(nxy1, Vector2f(box.minCoordinates.x(), box.minCoordinates.y())) + dxy1 < 0.0f ||
        Vector2f::dot(nxy2, Vector2f(box.minCoordinates.x(), box.minCoordinates.y())) + dxy2 < 0.0f)
    {
        return false;
    }

    // yz plane

    auto nyz0 = Vector2f(-e0.z(), e0.y());
    auto nyz1 = Vector2f(-e1.z(), e1.y());
    auto nyz2 = Vector2f(-e2.z(), e2.y());
    if (normal.x() < 0.0f)
    {
        nyz0 *= -1.0f;
        nyz1 *= -1.0f;
        nyz2 *= -1.0f;
    }

    auto dyz0 = -Vector2f::dot(nyz0, Vector2f(v0.y(), v0.z())) + std::max(0.0f, extents.y() * nyz0.x()) + std::max(0.0f, extents.z() * nyz0.y());
    auto dyz1 = -Vector2f::dot(nyz1, Vector2f(v1.y(), v1.z())) + std::max(0.0f, extents.y() * nyz1.x()) + std::max(0.0f, extents.z() * nyz1.y());
    auto dyz2 = -Vector2f::dot(nyz2, Vector2f(v2.y(), v2.z())) + std::max(0.0f, extents.y() * nyz2.x()) + std::max(0.0f, extents.z() * nyz2.y());

    if (Vector2f::dot(nyz0, Vector2f(box.minCoordinates.y(), box.minCoordinates.z())) + dyz0 < 0.0f ||
        Vector2f::dot(nyz1, Vector2f(box.minCoordinates.y(), box.minCoordinates.z())) + dyz1 < 0.0f ||
        Vector2f::dot(nyz2, Vector2f(box.minCoordinates.y(), box.minCoordinates.z())) + dyz2 < 0.0f)
    {
        return false;
    }

    // zx plane

    auto nzx0 = Vector2f(-e0.x(), e0.z());
    auto nzx1 = Vector2f(-e1.x(), e1.z());
    auto nzx2 = Vector2f(-e2.x(), e2.z());
    if (normal.y() < 0.0f)
    {
        nzx0 *= -1.0f;
        nzx1 *= -1.0f;
        nzx2 *= -1.0f;
    }

    auto dzx0 = -Vector2f::dot(nzx0, Vector2f(v0.z(), v0.x())) + std::max(0.0f, extents.z() * nzx0.x()) + std::max(0.0f, extents.x() * nzx0.y());
    auto dzx1 = -Vector2f::dot(nzx1, Vector2f(v1.z(), v1.x())) + std::max(0.0f, extents.z() * nzx1.x()) + std::max(0.0f, extents.x() * nzx1.y());
    auto dzx2 = -Vector2f::dot(nzx2, Vector2f(v2.z(), v2.x())) + std::max(0.0f, extents.z() * nzx2.x()) + std::max(0.0f, extents.x() * nzx2.y());

    if (Vector2f::dot(nzx0, Vector2f(box.minCoordinates.z(), box.minCoordinates.x())) + dzx0 < 0.0f ||
        Vector2f::dot(nzx1, Vector2f(box.minCoordinates.z(), box.minCoordinates.x())) + dzx1 < 0.0f ||
        Vector2f::dot(nzx2, Vector2f(box.minCoordinates.z(), box.minCoordinates.x())) + dzx2 < 0.0f)
    {
        return false;
    }

    return true;
}

}
