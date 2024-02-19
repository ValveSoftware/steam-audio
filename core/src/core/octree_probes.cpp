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

#include "octree_probes.h"
#include "octree.h"

namespace ipl {

void generateOctreeProbes(const IScene& scene, const Matrix4x4f& obbTransform, float spacing, ProbeArray& probes)
{
    // these are the vertices of the probe box in its local coordinate system
    Vector4f vertices[8];
    vertices[0] = Vector4f(-0.5f, -0.5f, -0.5f, 1.0f);
    vertices[1] = Vector4f(0.5f, -0.5f, -0.5f, 1.0f);
    vertices[2] = Vector4f(-0.5f, 0.5f, -0.5f, 1.0f);
    vertices[3] = Vector4f(-0.5f, -0.5f, 0.5f, 1.0f);
    vertices[4] = Vector4f(0.5f, 0.5f, -0.5f, 1.0f);
    vertices[5] = Vector4f(-0.5f, 0.5f, 0.5f, 1.0f);
    vertices[6] = Vector4f(0.5f, -0.5f, 0.5f, 1.0f);
    vertices[7] = Vector4f(0.5f, 0.5f, 0.5f, 1.0f);

    // transform the obb vertices to world space
    for (auto i = 0; i < 8; ++i)
    {
        vertices[i] = obbTransform * vertices[i];
    }

    // build an aabb that contains all vertices of the probe box
    Box box{};
    for (auto i = 0; i < 8; ++i)
    {
        Vector3f vertex(vertices[i].x(), vertices[i].y(), vertices[i].z());
        box.minCoordinates = Vector3f::min(box.minCoordinates, vertex);
        box.maxCoordinates = Vector3f::max(box.maxCoordinates, vertex);
    }

    // build an octree within the aabb
    Octree octree(scene, box, spacing);

    // figure out which octree nodes we want to use for creating probes
    vector<int32_t> nodeIndices;
    for (auto i = 0; i < octree.getNumNodes(); ++i)
    {
        // only use leaf nodes for probes
        if (!octree.getNode(i).isLeaf())
            continue;

        // if the probe lies outside the aabb / obb, ignore it
        // todo: should we check for containment within the aabb or the obb?
        // todo: should this be a box/box test?
        if (!box.contains(octree.getNode(i).box.center()))
            continue;

        // use this node
        nodeIndices.push_back(i);
    }

    // create probes from all the chosen nodes
    probes.probes.resize(nodeIndices.size());
    for (auto i = 0; i < nodeIndices.size(); ++i)
    {
        auto nodeIndex = nodeIndices[i];

        const auto& node = octree.getNode(nodeIndex);

        probes[i].influence.center = node.box.center();
        probes[i].influence.radius = 0.5f * node.box.extents().length();
    }
}

}
