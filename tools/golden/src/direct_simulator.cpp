// Golden data for stage 5.1: direct_simulator.{h,cpp}.
//
// Scenes are built from core/data/meshes (run from the repository root) with scene_inputs.h; every other input comes
// from Lcg draws in a fixed order, one value per statement. crates/steam-audio/tests/direct_simulator.rs replays them.
//
// Per scene, source/listener pairs (random points around the scene, then pairs on opposite sides of the instanced box,
// so that rays cross several surfaces) are simulated with several settings, one row of
// [distanceAttenuation, airAbsorption[3], delay, occlusion, transmission[3], directivity] each:
//   0: every flag, raycast occlusion, 1-16 transmission rays;
//   1: every flag, volumetric occlusion with a 64-point simulator (asking for 16, 64, 200 or 1 points);
//   2: every flag, volumetric occlusion with a 256-point simulator and a larger radius;
//   3: occlusion only, volumetric, with a 1-point simulator (which keeps no points);
//   4: flags = pair index % 64, alternating occlusion types, and no scene for every 7th pair.

#include <iterator>
#include <string>

#include <direct_simulator.h>
#include <instanced_mesh.h>
#include <scene.h>
#include <static_mesh.h>

#include "generators.h"
#include "npy.h"
#include "scene_inputs.h"

namespace {

using namespace golden;
using namespace ipl;

constexpr int kNumRandomPairs = 200;
constexpr int kNumCrossingPairs = 56;
constexpr int kTransmissionRays[] = {1, 2, 3, 4, 8, 16};
constexpr int kVolumetricSamples[] = {16, 64, 200, 1};
constexpr int kAllFlags = 0x3f;

void appendPath(std::vector<float>& rows, const DirectSoundPath& p)
{
    rows.push_back(p.distanceAttenuation);
    rows.insert(rows.end(), std::begin(p.airAbsorption), std::end(p.airAbsorption));
    rows.push_back(p.delay);
    rows.push_back(p.occlusion);
    rows.insert(rows.end(), std::begin(p.transmission), std::end(p.transmission));
    rows.push_back(p.directivity);
}

struct SceneCase
{
    const char* name;
    uint32_t seed;
};

const SceneCase kSceneCases[] = {
    {"boxroom", 501},
    {"simplescene", 502},
};

void generateDirectSimulator(const std::filesystem::path& out)
{
    for (const auto& sceneCase : kSceneCases)
    {
        const std::string name = sceneCase.name;
        Lcg lcg(sceneCase.seed);

        // The mesh with Lcg materials, plus an instance of smallbox (also with Lcg materials) inside its bounds.
        auto obj = loadObj(name);
        auto smallbox = loadObj("smallbox");
        assignMaterials(lcg, obj);
        assignMaterials(lcg, smallbox);

        auto boxScene = ipl::make_shared<Scene>();
        boxScene->addStaticMesh(createStaticMesh(*boxScene, smallbox));
        boxScene->commit();

        Scene scene;
        auto mesh = createStaticMesh(scene, obj);
        const auto bounds = static_cast<const StaticMesh&>(*mesh).boundingBox();
        const auto transform = drawTransform(lcg, bounds);
        scene.addStaticMesh(mesh);
        scene.addInstancedMesh(scene.createInstancedMesh(boxScene, transform));
        scene.commit();

        const auto diagonal = bounds.extents().length();
        const Vector3f boxCenter(transform(0, 3), transform(1, 3), transform(2, 3));

        const float dipoleWeight = draw01(lcg);
        const float dipolePower = draw01(lcg) * 4.0f;
        const DistanceAttenuationModel distanceModel(0.5f, nullptr, nullptr);
        const AirAbsorptionModel airModel{};
        const Directivity directivity(dipoleWeight, dipolePower);

        DirectSimulator simulator64(64);
        DirectSimulator simulator256(256);
        DirectSimulator simulator1(1);

        std::vector<float> rows;
        for (auto i = 0; i < kNumRandomPairs + kNumCrossingPairs; ++i)
        {
            const auto ahead = Vector3f::unitVector(drawVector(lcg));
            Vector3f sourcePosition;
            Vector3f listenerPosition;
            if (i < kNumRandomPairs)
            {
                sourcePosition = drawPoint(lcg, bounds);
                listenerPosition = drawPoint(lcg, bounds);
            }
            else
            {
                const auto offset = drawVector(lcg);
                const float scale = draw01(lcg);
                sourcePosition = boxCenter + offset * (0.3f * diagonal * (0.1f + scale));
                listenerPosition = boxCenter * 2.0f - sourcePosition;
            }
            const float radius = draw01(lcg) * 0.05f * diagonal;

            const CoordinateSpace3f source(ahead, sourcePosition);
            const CoordinateSpace3f listener(Vector3f::kZAxis, listenerPosition);
            const int numRays = kTransmissionRays[i % std::size(kTransmissionRays)];
            const int numSamples = kVolumetricSamples[i % std::size(kVolumetricSamples)];

            auto simulate = [&](DirectSimulator& simulator, const IScene* s, int flags, OcclusionType type, float r,
                                int samples) {
                DirectSoundPath path{};
                simulator.simulate(s, static_cast<DirectSimulationFlags>(flags), source, listener, distanceModel,
                                   airModel, directivity, type, r, samples, numRays, path);
                appendPath(rows, path);
            };

            simulate(simulator64, &scene, kAllFlags, OcclusionType::Raycast, radius, numSamples);
            simulate(simulator64, &scene, kAllFlags, OcclusionType::Volumetric, radius, numSamples);
            simulate(simulator256, &scene, kAllFlags, OcclusionType::Volumetric, radius * 4.0f, 256);
            simulate(simulator1, &scene, CalcOcclusion, OcclusionType::Volumetric, radius, 32);
            simulate(simulator64, (i % 7 == 0) ? nullptr : &scene, i % 64,
                     (i % 2 == 0) ? OcclusionType::Raycast : OcclusionType::Volumetric, radius, numSamples);
        }

        writeNpy(out / (name + ".npy"), {rows.size() / 50, 5, 10}, rows.data());
    }
}

Register directSimulator("direct_simulator", generateDirectSimulator);

}
