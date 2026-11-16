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

#include <catch.hpp>

#include <scene.h>

TEST_CASE("IScene", "[IScene]")
{
}

TEST_CASE("Scene", "[Scene]")
{
}

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))

#include <cmath>
#include <vector>

#include <embree_device.h>
#include <scene_factory.h>

using namespace ipl;

TEST_CASE("Hit normals of a rotated instanced mesh are in world space", "[Scene]")
{
    auto embree = make_shared<EmbreeDevice>();
    auto createScene = [&]()
    {
        return shared_ptr<IScene>(SceneFactory::create(SceneType::Embree, nullptr, nullptr, nullptr, nullptr, nullptr, embree, nullptr));
    };

    // A triangle in the plane z = -1; its normal is along the z axis.
    Vector3f vertices[] = { Vector3f{ -1.0f, -1.0f, -1.0f }, Vector3f{ 1.0f, -1.0f, -1.0f }, Vector3f{ 0.0f, 1.0f, -1.0f } };
    Triangle triangles[1];
    triangles[0].indices[0] = 0;
    triangles[0].indices[1] = 1;
    triangles[0].indices[2] = 2;
    int materialIndices[] = { 0 };
    Material materials[1] = {};

    auto subScene = createScene();
    subScene->addStaticMesh(subScene->createStaticMesh(3, 1, 1, vertices, triangles, materialIndices, materials));
    subScene->commit();

    // Rotate by 90 degrees around the y axis (z -> -x), which puts the triangle in the plane x = -1.
    auto rotation = Matrix4x4f::identityMatrix();
    rotation(0, 0) = 0.0f;
    rotation(0, 2) = 1.0f;
    rotation(2, 0) = -1.0f;
    rotation(2, 2) = 0.0f;

    auto scene = createScene();
    scene->addInstancedMesh(scene->createInstancedMesh(subScene, rotation));
    scene->commit();

    Ray ray{ Vector3f::kZero, -Vector3f::kXAxis };
    auto hit = scene->closestHit(ray, 0.0f, 10.0f);
    REQUIRE(hit.isValid());
    REQUIRE(hit.distance == Approx(1.0f));
    INFO("normal: " << hit.normal.x() << " " << hit.normal.y() << " " << hit.normal.z());
    REQUIRE(fabsf(hit.normal.x()) > 0.99f);
}

TEST_CASE("Hit normals of an instanced mesh with a rotation, a non-uniform scale and a mirror match the same mesh built directly", "[Scene]")
{
    auto embree = make_shared<EmbreeDevice>();
    auto createScene = [&]()
    {
        return shared_ptr<IScene>(SceneFactory::create(SceneType::Embree, nullptr, nullptr, nullptr, nullptr, nullptr, embree, nullptr));
    };

    // A box, and its 6 faces (2 triangles each).
    const Vector3f lo{ -2.0f, -1.5f, -1.2f };
    const Vector3f hi{ 2.0f, 1.5f, 1.2f };
    std::vector<Vector3f> vertices;
    for (auto i = 0; i < 8; ++i)
    {
        vertices.push_back(Vector3f{ (i & 1) ? hi.x() : lo.x(), (i & 2) ? hi.y() : lo.y(), (i & 4) ? hi.z() : lo.z() });
    }

    const int quads[6][4] = { {0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 5, 7, 6} };
    std::vector<Triangle> triangles;
    for (const auto& quad : quads)
    {
        triangles.push_back(Triangle{ { quad[0], quad[1], quad[2] } });
        triangles.push_back(Triangle{ { quad[0], quad[2], quad[3] } });
    }

    std::vector<int> materialIndices(triangles.size(), 0);
    Material materials[1] = {};

    for (auto mirrored : { false, true })
    {
        // A rotation of 37 degrees around the axis (1, 2, 3), then a non-uniform scale (a mirror in x if requested),
        // then a translation. The inverse transpose of this matrix is neither the matrix nor its transpose.
        const auto axis = Vector3f::unitVector(Vector3f{ 1.0f, 2.0f, 3.0f });
        const auto angle = 37.0f * 3.14159265f / 180.0f;
        const auto c = std::cos(angle);
        const auto s = std::sin(angle);
        const float scale[3] = { mirrored ? -1.0f : 1.0f, 1.3f, 0.8f };

        auto transform = Matrix4x4f::identityMatrix();
        for (auto row = 0; row < 3; ++row)
        {
            for (auto col = 0; col < 3; ++col)
            {
                transform(row, col) = ((row == col ? c : 0.0f) + (1.0f - c) * axis.elements[row] * axis.elements[col]) * scale[col];
            }
        }

        const float cross[3][3] = { {0.0f, -axis.z(), axis.y()}, {axis.z(), 0.0f, -axis.x()}, {-axis.y(), axis.x(), 0.0f} };
        for (auto row = 0; row < 3; ++row)
        {
            for (auto col = 0; col < 3; ++col)
            {
                transform(row, col) += s * cross[row][col] * scale[col];
            }
        }

        transform(0, 3) = 0.2f;
        transform(1, 3) = -0.1f;
        transform(2, 3) = 0.05f;

        auto apply = [&](const Vector3f& p)
        {
            return Vector3f{ transform(0, 0) * p.x() + transform(0, 1) * p.y() + transform(0, 2) * p.z() + transform(0, 3),
                             transform(1, 0) * p.x() + transform(1, 1) * p.y() + transform(1, 2) * p.z() + transform(1, 3),
                             transform(2, 0) * p.x() + transform(2, 1) * p.y() + transform(2, 2) * p.z() + transform(2, 3) };
        };

        std::vector<Vector3f> transformedVertices;
        for (const auto& vertex : vertices)
        {
            transformedVertices.push_back(apply(vertex));
        }

        auto direct = createScene();
        direct->addStaticMesh(direct->createStaticMesh(8, 12, 1, transformedVertices.data(), triangles.data(), materialIndices.data(), materials));
        direct->commit();

        auto subScene = createScene();
        subScene->addStaticMesh(subScene->createStaticMesh(8, 12, 1, vertices.data(), triangles.data(), materialIndices.data(), materials));
        subScene->commit();

        auto instanced = createScene();
        instanced->addInstancedMesh(instanced->createInstancedMesh(subScene, transform));
        instanced->commit();

        // Shoot a ray from near the center of the box to the center of each face.
        const auto origin = apply(Vector3f{ 0.01f, 0.02f, 0.03f });
        for (auto face = 0; face < 6; ++face)
        {
            const auto& triangle = triangles[2 * face];
            const auto center = (vertices[triangle.indices[0]] + vertices[triangle.indices[1]] + vertices[triangle.indices[2]]) * (1.0f / 3.0f);
            const Ray ray{ origin, Vector3f::unitVector(apply(center) - origin) };

            const auto directHit = direct->closestHit(ray, 0.0f, 100.0f);
            const auto instancedHit = instanced->closestHit(ray, 0.0f, 100.0f);
            REQUIRE(directHit.isValid());
            REQUIRE(instancedHit.isValid());
            REQUIRE(instancedHit.distance == Approx(directHit.distance));

            // The sign of the normal may differ with a mirror; the direction must not.
            INFO("face " << face << (mirrored ? " (mirrored)" : ""));
            REQUIRE(std::fabs(Vector3f::dot(directHit.normal, instancedHit.normal)) > 0.9999f);
        }
    }
}

#endif
