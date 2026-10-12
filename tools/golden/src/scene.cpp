// Golden data for stage 5: mesh.{h,cpp}, bvh.{h,cpp}, static_mesh.{h,cpp}, instanced_mesh.{h,cpp}, scene.{h,cpp}.
//
// Meshes are read from core/data/meshes (run from the repository root) with the OBJ loader in scene_inputs.h. Other
// inputs come from Lcg draws in a fixed order, one value per statement.
//
// Modules:
// - bvh: per mesh, the BVH nodes (in full for the small meshes, as an FNV-1a hash for teapot and sponza), triangle
//   normals, closest hits, any hits, occlusion between points and box queries for Lcg rays and boxes.
// - scene: a scene with a static mesh and instanced meshes, traced after each of a series of changes (transform
//   updates, material changes, removal), with its version after every step, and its dumpObj output.

#include <cstring>
#include <limits>
#include <memory>
#include <string>

#include <instanced_mesh.h>
#include <scene.h>
#include <static_mesh.h>

#include "generators.h"
#include "npy.h"
#include "scene_inputs.h"

namespace {

using namespace golden;
using namespace ipl;

// --------------------------------------------------------------------------------------------------------------------
// Inputs
// --------------------------------------------------------------------------------------------------------------------

struct RayInput
{
    Ray ray;
    float minDistance;
    float maxDistance;
};

// Every 16th ray is axis-aligned (cycling through +x, -x, +y, -y, +z, -z), with its other components +0 or -0, to
// exercise the infinite reciprocals of zero components.
RayInput drawRay(Lcg& lcg, const Box& box, int i)
{
    const auto diagonal = box.extents().length();

    RayInput input;
    input.ray.origin = drawPoint(lcg, box);

    if (i % 16 == 0)
    {
        const auto k = i / 16;
        const auto zero = (k / 6) % 2 == 0 ? 0.0f : -0.0f;
        input.ray.direction = Vector3f(zero, zero, zero);
        input.ray.direction[(k % 6) / 2] = (k % 2 == 0) ? 1.0f : -1.0f;
    }
    else
    {
        input.ray.direction = Vector3f::unitVector(drawVector(lcg));
    }

    input.minDistance = (i % 3 == 0) ? 0.0f : draw01(lcg) * 0.1f * diagonal;
    input.maxDistance = (i % 4 == 0) ? std::numeric_limits<float>::infinity() : draw01(lcg) * diagonal;
    return input;
}

Box drawBox(Lcg& lcg, const Box& bounds)
{
    const auto center = drawPoint(lcg, bounds);
    const auto size = draw01(lcg) * 0.05f * bounds.extents().length();
    const auto half = Vector3f(size, size, size);
    return Box(center - half, center + half);
}

// --------------------------------------------------------------------------------------------------------------------
// Output
// --------------------------------------------------------------------------------------------------------------------

struct Fnv1a
{
    uint64_t hash = 0xcbf29ce484222325ull;

    void add(const void* data, size_t size)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 0x100000001b3ull;
        }
    }
};

struct HitRows
{
    std::vector<float> closest;       // distance, normal, absorption[3], scattering, transmission[3] (NaN: no material)
    std::vector<int32_t> indices;     // triangle, material
    std::vector<int32_t> any;
    std::vector<int32_t> occluded;    // isOccluded(origin, origin + direction * (finite max distance or 10))

    void trace(const IScene& scene, const RayInput& input)
    {
        const auto hit = scene.closestHit(input.ray, input.minDistance, input.maxDistance);
        closest.insert(closest.end(), {hit.distance, hit.normal.x(), hit.normal.y(), hit.normal.z()});
        if (hit.material)
        {
            const auto& m = *hit.material;
            closest.insert(closest.end(), {m.absorption[0], m.absorption[1], m.absorption[2], m.scattering,
                                           m.transmission[0], m.transmission[1], m.transmission[2]});
        }
        else
        {
            closest.insert(closest.end(), 7, std::numeric_limits<float>::quiet_NaN());
        }
        indices.insert(indices.end(), {hit.triangleIndex, hit.materialIndex});

        any.push_back(scene.anyHit(input.ray, input.minDistance, input.maxDistance) ? 1 : 0);

        const auto length = (input.maxDistance < std::numeric_limits<float>::infinity()) ? input.maxDistance : 10.0f;
        const auto end = input.ray.origin + length * input.ray.direction;
        occluded.push_back(scene.isOccluded(input.ray.origin, end) ? 1 : 0);
    }

    void write(const std::filesystem::path& out, const std::string& prefix) const
    {
        writeNpy(out / (prefix + "_closest.npy"), {any.size(), 11}, closest.data());
        writeNpy(out / (prefix + "_indices.npy"), {any.size(), 2}, indices.data());
        writeNpy(out / (prefix + "_any.npy"), any);
        writeNpy(out / (prefix + "_occluded.npy"), occluded);
    }
};

// --------------------------------------------------------------------------------------------------------------------
// bvh
// --------------------------------------------------------------------------------------------------------------------

struct MeshCase
{
    const char* name;
    uint32_t seed;
    int numRays;
    int numBoxes;
    bool fullNodes;
};

const MeshCase kMeshCases[] = {
    {"smallbox", 11, 509, 131, true},
    {"boxroom", 12, 509, 131, true},
    {"simplescene", 13, 509, 131, true},
    {"sphere", 14, 509, 131, true},
    {"teapot", 15, 1021, 257, false},
    {"sponza", 16, 2039, 509, false},
};

void generateBvh(const std::filesystem::path& out)
{
    for (const auto& meshCase : kMeshCases)
    {
        const std::string name = meshCase.name;
        const auto obj = loadObj(name);

        Scene scene;
        auto mesh = createStaticMesh(scene, obj);
        scene.addStaticMesh(mesh);
        scene.commit();
        const auto& staticMesh = static_cast<const StaticMesh&>(*mesh);

        // StaticMesh does not expose its BVH; this one is built from the same mesh, by the same code.
        BVH bvh(staticMesh.mesh());

        Fnv1a hash;
        std::vector<float> nodes;
        std::vector<int32_t> nodeData;
        for (auto i = 0; i < bvh.numNodes(); ++i)
        {
            const auto& box = bvh.node(i).boundingBox();
            const float coordinates[6] = {box.minCoordinates.x(), box.minCoordinates.y(), box.minCoordinates.z(),
                                          box.maxCoordinates.x(), box.maxCoordinates.y(), box.maxCoordinates.z()};
            int32_t data;
            std::memcpy(&data, &box.minCoordinates.elements[3], sizeof(data));

            hash.add(coordinates, sizeof(coordinates));
            hash.add(&data, sizeof(data));
            nodes.insert(nodes.end(), coordinates, coordinates + 6);
            nodeData.push_back(data);
        }

        const int32_t hashWords[3] = {static_cast<int32_t>(hash.hash & 0xffffffffu),
                                      static_cast<int32_t>(hash.hash >> 32), bvh.numNodes()};
        writeNpy(out / (name + "_nodes_hash.npy"), {3}, hashWords);

        if (meshCase.fullNodes)
        {
            writeNpy(out / (name + "_nodes.npy"), {nodeData.size(), 6}, nodes.data());
            writeNpy(out / (name + "_node_data.npy"), nodeData);

            std::vector<float> normals;
            for (auto i = 0; i < staticMesh.numTriangles(); ++i)
            {
                const auto& n = staticMesh.mesh().normal(i);
                normals.insert(normals.end(), {n.x(), n.y(), n.z()});
            }
            writeNpy(out / (name + "_normals.npy"), {normals.size() / 3, 3}, normals.data());
        }

        Lcg lcg(meshCase.seed);
        const auto bounds = staticMesh.boundingBox();

        HitRows rows;
        for (auto i = 0; i < meshCase.numRays; ++i)
            rows.trace(scene, drawRay(lcg, bounds, i));
        rows.write(out, name);

        std::vector<int32_t> boxes;
        for (auto i = 0; i < meshCase.numBoxes; ++i)
            boxes.push_back(staticMesh.intersectsBox(drawBox(lcg, bounds)) ? 1 : 0);
        writeNpy(out / (name + "_boxes.npy"), boxes);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// scene
// --------------------------------------------------------------------------------------------------------------------

void writeBytes(const std::filesystem::path& path, const std::filesystem::path& source)
{
    std::ifstream in(source, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    writeNpy(path, bytes);
    std::filesystem::remove(source);
}

void generateScene(const std::filesystem::path& out)
{
    Lcg lcg(20);

    auto boxroom = loadObj("boxroom");
    auto smallbox = loadObj("smallbox");
    auto sphere = loadObj("sphere");
    assignMaterials(lcg, boxroom);
    assignMaterials(lcg, smallbox);
    assignMaterials(lcg, sphere);

    auto boxScene = ipl::make_shared<Scene>();
    boxScene->addStaticMesh(createStaticMesh(*boxScene, smallbox));
    boxScene->commit();

    auto sphereScene = ipl::make_shared<Scene>();
    sphereScene->addStaticMesh(createStaticMesh(*sphereScene, sphere));
    sphereScene->commit();

    Scene scene;
    auto room = createStaticMesh(scene, boxroom);
    const auto bounds = static_cast<const StaticMesh&>(*room).boundingBox();

    const auto transform0 = drawTransform(lcg, bounds);
    const auto transform1 = drawTransform(lcg, bounds);
    const auto transform2 = drawTransform(lcg, bounds);
    const auto transform1b = drawTransform(lcg, bounds);
    auto instance0 = scene.createInstancedMesh(boxScene, transform0);
    auto instance1 = scene.createInstancedMesh(sphereScene, transform1);
    auto instance2 = scene.createInstancedMesh(boxScene, transform2);

    const auto newMaterial = drawMaterial(lcg);

    std::vector<int32_t> versions;
    auto record = [&]() {
        versions.insert(versions.end(), {static_cast<int32_t>(scene.version()), scene.numStaticMeshes(),
                                         scene.numInstancedMeshes()});
    };

    std::vector<RayInput> rays;
    for (auto i = 0; i < 509; ++i)
        rays.push_back(drawRay(lcg, bounds, i));

    // Rays aimed near the instances' origins (the translations, before transform1b moves instance 1).
    const Matrix4x4f* transforms[3] = {&transform0, &transform1, &transform2};
    for (auto i = 0; i < 256; ++i)
    {
        const auto& t = *transforms[i % 3];
        const auto origin = drawPoint(lcg, bounds);
        const auto offset = drawVector(lcg);
        const auto target = Vector3f(t(0, 3), t(1, 3), t(2, 3)) + offset * 0.5f;
        RayInput input;
        input.ray.origin = origin;
        input.ray.direction = Vector3f::unitVector(target - origin);
        input.minDistance = 0.0f;
        input.maxDistance = std::numeric_limits<float>::infinity();
        rays.push_back(input);
    }

    std::vector<Box> boxes;
    for (auto i = 0; i < 131; ++i)
        boxes.push_back(drawBox(lcg, bounds));

    int stage = 0;
    auto trace = [&]() {
        HitRows rows;
        for (const auto& ray : rays)
            rows.trace(scene, ray);
        rows.write(out, "stage" + std::to_string(stage));

        std::vector<Ray> batchRays;
        std::vector<float> minDistances, maxDistances;
        for (auto i = 0u; i < rays.size(); ++i)
        {
            batchRays.push_back(rays[i].ray);
            minDistances.push_back(rays[i].minDistance);
            maxDistances.push_back((i % 5 == 0) ? -1.0f : rays[i].maxDistance);
        }
        std::unique_ptr<bool[]> occluded(new bool[rays.size()]);
        scene.anyHits(static_cast<int>(rays.size()), batchRays.data(), minDistances.data(), maxDistances.data(),
                      occluded.get());
        std::vector<int32_t> anyHits;
        for (auto i = 0u; i < rays.size(); ++i)
            anyHits.push_back(occluded[i] ? 1 : 0);
        writeNpy(out / ("stage" + std::to_string(stage) + "_any_hits.npy"), anyHits);

        std::vector<int32_t> boxHits;
        for (const auto& box : boxes)
            boxHits.push_back(scene.intersectsBox(box) ? 1 : 0);
        writeNpy(out / ("stage" + std::to_string(stage) + "_boxes.npy"), boxHits);

        ++stage;
    };

    // Step 0: nothing committed yet.
    scene.addStaticMesh(room);
    scene.addInstancedMesh(instance0);
    scene.addInstancedMesh(instance1);
    scene.addInstancedMesh(instance2);
    record();
    trace(); // stage 0: empty

    scene.commit();
    record();
    trace(); // stage 1: everything

    scene.commit();
    record();

    instance0->updateTransform(scene, transform0); // Same transform: no change.
    scene.commit();
    record();

    instance1->updateTransform(scene, transform1b);
    record();
    trace(); // stage 2: the new transform is used before the commit
    scene.commit();
    record();

    scene.setStaticMeshMaterial(room.get(), const_cast<Material*>(&newMaterial), 1);
    scene.commit();
    record();
    trace(); // stage 3: material 1 of the room replaced

    scene.removeStaticMesh(room);
    record();
    trace(); // stage 4: removal not committed yet
    scene.commit();
    record();
    trace(); // stage 5: instances only

    scene.removeInstancedMesh(instance2);
    scene.addStaticMesh(room);
    scene.addStaticMesh(room);
    scene.commit();
    record();
    trace(); // stage 6: the room twice, two instances

    writeNpy(out / "versions.npy", {versions.size() / 3, 3}, versions.data());

    scene.dumpObj(ipl::string((out / "dump_tmp.obj").string().c_str()));
    writeBytes(out / "dump_obj.npy", out / "dump_tmp.obj");
    writeBytes(out / "dump_mtl.npy", out / "dump_tmp.mtl");
}

Register bvh("bvh", generateBvh);
Register scene("scene", generateScene);

}
