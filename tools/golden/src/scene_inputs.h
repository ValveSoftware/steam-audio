#pragma once

// Scene inputs shared by the stage 5 generators (scene.cpp, direct_simulator.cpp): meshes read from core/data/meshes
// (run from the repository root) with the OBJ loader of upstream's RayTracerCompare.test.cpp, reproduced token for
// token by crates/steam-audio/tests/common/mod.rs, and Lcg draws of points, materials and transforms.

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <box.h>
#include <quaternion.h>
#include <scene.h>

#include "npy.h"

namespace golden {

// --------------------------------------------------------------------------------------------------------------------
// OBJ loading (RayTracerCompare.test.cpp's loadMesh, without the scene)
// --------------------------------------------------------------------------------------------------------------------

struct ObjMesh
{
    std::vector<ipl::Vector3f> vertices;
    std::vector<int> triangles;
    std::vector<int> materialIndices;
    std::vector<ipl::Material> materials;
};

inline ObjMesh loadObj(const std::string& name)
{
    using namespace ipl;

    ObjMesh mesh;

    std::ifstream mtlfile("core/data/meshes/" + name + ".mtl");
    if (!mtlfile.good())
        throw std::runtime_error("cannot open " + name + ".mtl (run from the repository root)");

    std::unordered_map<std::string, int> materialIndices;
    std::string currentmtl;

    float kd[3];
    float ks[3];

    while (true)
    {
        std::string command;
        mtlfile >> command;

        if (mtlfile.eof())
            break;

        if (command == "newmtl")
        {
            std::string mtlname;
            mtlfile >> mtlname;

            currentmtl = mtlname;

            mesh.materials.push_back(Material());
            materialIndices[mtlname] = static_cast<int>(mesh.materials.size()) - 1;
        }
        else if (command == "Kd")
        {
            mtlfile >> kd[0] >> kd[1] >> kd[2];

            for (auto i = 0; i < 3; ++i)
                mesh.materials[materialIndices[currentmtl]].absorption[i] = 1.0f - kd[i];

            mesh.materials[materialIndices[currentmtl]].scattering = 1.0f;
        }
        else if (command == "Ks")
        {
            mtlfile >> ks[0] >> ks[1] >> ks[2];

            for (auto i = 0; i < 3; ++i)
                mesh.materials[materialIndices[currentmtl]].absorption[i] = 1.0f - kd[i];

            mesh.materials[materialIndices[currentmtl]].scattering = 1.0f;
        }
    }

    std::ifstream file("core/data/meshes/" + name + ".obj");
    if (!file.good())
        throw std::runtime_error("cannot open " + name + ".obj (run from the repository root)");

    while (true)
    {
        std::string command;
        file >> command;

        if (file.eof())
            break;

        if (command == "v")
        {
            float x, y, z;
            file >> x >> y >> z;
            mesh.vertices.push_back(Vector3f(x, y, z));
        }
        else if (command == "f")
        {
            std::string s[3];
            file >> s[0] >> s[1] >> s[2];

            for (auto i = 0; i < 3; ++i)
                mesh.triangles.push_back(atoi(s[i].substr(0, s[i].find_first_of("/")).c_str()) - 1);

            if (materialIndices.find(currentmtl) == materialIndices.end())
                throw std::runtime_error(name + ": face without a material");

            mesh.materialIndices.push_back(materialIndices[currentmtl]);
        }
        else if (command == "usemtl")
        {
            file >> currentmtl;
        }
    }

    return mesh;
}

inline int numTriangles(const ObjMesh& mesh)
{
    return static_cast<int>(mesh.triangles.size()) / 3;
}

inline ipl::shared_ptr<ipl::IStaticMesh> createStaticMesh(ipl::IScene& scene, const ObjMesh& mesh)
{
    return scene.createStaticMesh(static_cast<int>(mesh.vertices.size()), numTriangles(mesh),
                                  static_cast<int>(mesh.materials.size()), mesh.vertices.data(),
                                  reinterpret_cast<const ipl::Triangle*>(mesh.triangles.data()),
                                  mesh.materialIndices.data(), mesh.materials.data());
}

// --------------------------------------------------------------------------------------------------------------------
// Lcg draws
// --------------------------------------------------------------------------------------------------------------------

// Uniform in [0, 1).
inline float draw01(Lcg& lcg)
{
    const float u = lcg.nextSigned();
    return 0.5f * (u + 1.0f);
}

inline ipl::Vector3f drawVector(Lcg& lcg)
{
    const float x = lcg.nextSigned();
    const float y = lcg.nextSigned();
    const float z = lcg.nextSigned();
    return ipl::Vector3f(x, y, z);
}

// A point in the box grown by 5% of its extents on every side.
inline ipl::Vector3f drawPoint(Lcg& lcg, const ipl::Box& box)
{
    const auto extents = box.extents();
    ipl::Vector3f p;
    for (auto i = 0; i < 3; ++i)
    {
        const float t = draw01(lcg) * 1.1f - 0.05f;
        p[i] = box.minCoordinates[i] + extents[i] * t;
    }
    return p;
}

inline ipl::Material drawMaterial(Lcg& lcg)
{
    ipl::Material m;
    for (auto i = 0; i < 3; ++i)
        m.absorption[i] = draw01(lcg);
    m.scattering = draw01(lcg);
    for (auto i = 0; i < 3; ++i)
        m.transmission[i] = draw01(lcg);
    return m;
}

// Replaces the OBJ's materials with three Lcg ones, assigned to triangles in turn.
inline void assignMaterials(Lcg& lcg, ObjMesh& mesh)
{
    mesh.materials.clear();
    for (auto i = 0; i < 3; ++i)
        mesh.materials.push_back(drawMaterial(lcg));

    for (auto i = 0; i < numTriangles(mesh); ++i)
        mesh.materialIndices[i] = i % 3;
}

// Translation * rotation * scale, with the translation inside `bounds` and the scale up to a tenth of its size.
inline ipl::Matrix4x4f drawTransform(Lcg& lcg, const ipl::Box& bounds)
{
    using namespace ipl;

    Quaternionf q;
    q.x = lcg.nextSigned();
    q.y = lcg.nextSigned();
    q.z = lcg.nextSigned();
    q.w = lcg.nextSigned();
    q.normalize();
    const auto rotation = q.toRotationMatrix();

    const auto size = bounds.extents().length();
    Vector3f scale;
    for (auto i = 0; i < 3; ++i)
        scale[i] = (0.2f + draw01(lcg)) * 0.1f * size;

    const auto translation = drawPoint(lcg, bounds);

    Matrix4x4f m;
    for (auto i = 0; i < 4; ++i)
        for (auto j = 0; j < 4; ++j)
            m(i, j) = 0.0f;
    for (auto i = 0; i < 3; ++i)
    {
        for (auto j = 0; j < 3; ++j)
            m(i, j) = rotation(i, j) * scale[j];
        m(i, 3) = translation[i];
    }
    m(3, 3) = 1.0f;
    return m;
}

}
