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

// A source with the StaticListener variation looks up its baked data at its own position. The probes it used must not
// leak into the lookups of the sources processed after it.
TEST_CASE("Baked lookups use the listener probes for sources that are not StaticListener", "[Pathing]")
{
    auto scene = makeScene({}, {});

    SimulationManager manager(false, true, false, SceneType::Default, IndirectEffectType::Parametric, 32, 1024, 32,
                              1.0f, 1, 4, 1, 1, 1, 8, true, Vector3f(0.0f, -1.0f, 0.0f), 48000, 1024,
                              nullptr, nullptr, nullptr);
    manager.scene() = scene;

    // Probe 0 is at the listener, probe 1 is far away, at the static listener source.
    auto probes = make_shared<ProbeBatch>();
    probes->addProbe(Sphere(Vector3f(0.0f, 0.0f, 0.0f), 5.0f));
    probes->addProbe(Sphere(Vector3f(100.0f, 0.0f, 0.0f), 5.0f));
    probes->commit();

    BakedDataIdentifier reverbId;
    reverbId.type = BakedDataType::Reflections;
    reverbId.variation = BakedDataVariation::Reverb;

    BakedDataIdentifier staticListenerId;
    staticListenerId.type = BakedDataType::Reflections;
    staticListenerId.variation = BakedDataVariation::StaticListener;
    staticListenerId.endpointInfluence = Sphere(Vector3f(0.0f, 0.0f, 0.0f), 5.0f);

    auto setReverbTimes = [&](const BakedDataIdentifier& id, float timeAtProbe0, float timeAtProbe1)
    {
        auto data = make_unique<BakedReflectionsData>(id, probes->numProbes(), false, true);
        Reverb r0;
        Reverb r1;
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            r0.reverbTimes[i] = timeAtProbe0;
            r1.reverbTimes[i] = timeAtProbe1;
        }
        data->set(0, r0);
        data->set(1, r1);
        probes->addData(id, std::move(data));
    };

    setReverbTimes(reverbId, 1.0f, 2.0f);
    setReverbTimes(staticListenerId, 1.0f, 2.0f);

    manager.addProbeBatch(probes);

    auto makeSource = [&](const BakedDataIdentifier& id, const Vector3f& position)
    {
        auto source = make_shared<SimulationData>(true, false, SceneType::Default, IndirectEffectType::Parametric, 32, 1.0f, 1, 48000, 1024, nullptr, nullptr);
        source->reflectionInputs.enabled = true;
        source->reflectionInputs.baked = true;
        source->reflectionInputs.bakedDataIdentifier = id;
        source->reflectionInputs.source = CoordinateSpace3f(position);
        manager.addSource(source);
        return source;
    };

    // The first source is a static listener source at the far probe; the second one is the reverb at the listener.
    auto staticListenerSource = makeSource(staticListenerId, Vector3f(100.0f, 0.0f, 0.0f));
    auto reverbSource = makeSource(reverbId, Vector3f(0.0f, 0.0f, 0.0f));

    manager.commit();

    SharedReflectionSimulationInputs shared;
    shared.listener = CoordinateSpace3f(Vector3f(0.0f, 0.0f, 0.0f));
    shared.numRays = 1024;
    shared.numBounces = 4;
    shared.duration = 1.0f;
    shared.order = 1;
    shared.irradianceMinDistance = 1.0f;
    shared.reconstructionType = ReconstructionType::Linear;
    manager.setSharedReflectionInputs(shared);

    manager.simulateIndirect();

    INFO("static listener source reverb time: " << staticListenerSource->reflectionOutputs.reverb.reverbTimes[0] << " (expected 2)");
    INFO("reverb source reverb time: " << reverbSource->reflectionOutputs.reverb.reverbTimes[0] << " (expected 1)");

    REQUIRE(staticListenerSource->reflectionOutputs.reverb.reverbTimes[0] == Approx(2.0f));
    REQUIRE(reverbSource->reflectionOutputs.reverb.reverbTimes[0] == Approx(1.0f));
}
