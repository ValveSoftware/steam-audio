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

#include <baked_reflection_data.h>
#include <path_visibility.h>
#include <sampling.h>
#include <scene_factory.h>
#include <simulation_data.h>
#include <simulation_manager.h>

using namespace ipl;


namespace
{

// Adds an axis-aligned quad in a plane of constant z, spanning [x0, x1] x [y0, y1], to the vertex and triangle lists.
void addQuadZ(vector<Vector3f>& vertices, vector<Triangle>& triangles, float x0, float x1, float y0, float y1, float z)
{
    auto base = static_cast<int>(vertices.size());
    vertices.push_back(Vector3f(x0, y0, z));
    vertices.push_back(Vector3f(x1, y0, z));
    vertices.push_back(Vector3f(x1, y1, z));
    vertices.push_back(Vector3f(x0, y1, z));

    Triangle a;
    a.indices[0] = base + 0; a.indices[1] = base + 1; a.indices[2] = base + 2;
    Triangle b;
    b.indices[0] = base + 0; b.indices[1] = base + 2; b.indices[2] = base + 3;
    triangles.push_back(a);
    triangles.push_back(b);
}

shared_ptr<IScene> makeScene(const vector<Vector3f>& vertices, const vector<Triangle>& triangles)
{
    shared_ptr<IScene> scene(SceneFactory::create(SceneType::Default, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));

    if (!triangles.empty())
    {
        vector<int> materialIndices(triangles.size(), 0);
        Material materials[1] = {};
        scene->addStaticMesh(scene->createStaticMesh(static_cast<int>(vertices.size()), static_cast<int>(triangles.size()), 1,
                                                     vertices.data(), triangles.data(), materialIndices.data(), materials));
    }

    scene->commit();
    return scene;
}

}

// The documentation says two probes are mutually visible when "the fraction of unoccluded rays is at least
// threshold". numSamples * numSamples rays are traced (one per pair of samples), so the fraction has to be taken over
// numSamples * numSamples, not over numSamples.
TEST_CASE("Probe visibility threshold is a fraction of all traced rays", "[Pathing]")
{
    // A wall in the plane z = 0, with a narrow vertical slit in the middle.
    vector<Vector3f> vertices;
    vector<Triangle> triangles;
    const auto slit = 0.15f;
    addQuadZ(vertices, triangles, -20.0f, -slit, -20.0f, 20.0f, 0.0f);
    addQuadZ(vertices, triangles, slit, 20.0f, -20.0f, 20.0f, 0.0f);
    auto scene = makeScene(vertices, triangles);

    ProbeBatch probes;
    probes.addProbe(Sphere(Vector3f(0.0f, 0.0f, -3.0f), 1.0f));
    probes.addProbe(Sphere(Vector3f(0.0f, 0.0f, 3.0f), 1.0f));

    const auto numSamples = 8;
    const auto radius = 1.0f;

    // Count the unoccluded rays the same way the tester does, but over the full numSamples^2 rays.
    Vector3f samples[numSamples];
    Sampling::generateSphereVolumeSamples(numSamples, samples);

    auto numVisible = 0;
    for (auto i = 0; i < numSamples; ++i)
    {
        auto from = Sampling::transformSphereVolumeSample(samples[i], Sphere(probes[0].influence.center, radius));
        if (scene->isOccluded(probes[0].influence.center, from))
            continue;

        for (auto j = 0; j < numSamples; ++j)
        {
            auto to = Sampling::transformSphereVolumeSample(samples[j], Sphere(probes[1].influence.center, radius));
            if (scene->isOccluded(probes[1].influence.center, to))
                continue;

            if (!scene->isOccluded(from, to))
                ++numVisible;
        }
    }

    auto fraction = static_cast<float>(numVisible) / static_cast<float>(numSamples * numSamples);
    INFO("visible rays: " << numVisible << " of " << numSamples * numSamples << " (fraction " << fraction << ")");

    const auto threshold = 0.5f;

    // The scene was built so that the real fraction is well below the threshold, but not zero.
    REQUIRE(numVisible > 0);
    REQUIRE(fraction < threshold);

    ProbeVisibilityTester tester(numSamples, false, Vector3f(0.0f, -1.0f, 0.0f));
    REQUIRE_FALSE(tester.areProbesVisible(*scene, probes, 0, 1, radius, threshold));

    // And with an open wall, every ray gets through, so the probes are visible whatever the threshold.
    auto emptyScene = makeScene({}, {});
    REQUIRE(tester.areProbesVisible(*emptyScene, probes, 0, 1, radius, 1.0f));
}
