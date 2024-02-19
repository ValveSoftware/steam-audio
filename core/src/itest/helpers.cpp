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

#include "helpers.h"

#include <tchar.h>
#include <strsafe.h>
#include <locale>
#include <codecvt>
#include <fstream>

#include <Windows.h>


std::shared_ptr<HRTFDatabase> loadHRTF(shared_ptr<Context> context,
                                       float volume,
                                       HRTFNormType normType,
                                       int samplingRate,
                                       int frameSize,
                                       const char* sofaFileName)
{
    HRTFSettings hrtfSettings{};
    hrtfSettings.volume = volume;
    hrtfSettings.normType = normType;

    if (sofaFileName)
    {
        hrtfSettings.type = HRTFMapType::SOFA;
        hrtfSettings.sofaFileName = sofaFileName;
    }

    return std::make_shared<HRTFDatabase>(hrtfSettings, samplingRate, frameSize);
}

std::shared_ptr<IScene> loadMesh(shared_ptr<Context> context,
                                 const std::string& fileName,
                                 const std::string& materialFileName,
                                 const SceneType& sceneType,
                                 ClosestHitCallback closestHit,
                                 AnyHitCallback anyHit,
                                 BatchedClosestHitCallback batchedClosestHit,
                                 BatchedAnyHitCallback batchedAnyHit,
                                 void* userData,
                                 shared_ptr<EmbreeDevice> embree,
								 shared_ptr<ipl::RadeonRaysDevice> radeonRays)
{
    std::ifstream mtlfile((std::string{"../../data/meshes/"} +materialFileName).c_str());
    if (!mtlfile.good())
    {
        printf("unable to find mtl file.");
        exit(0);
    }

    std::vector<Material> materials;
    std::unordered_map<std::string, int> materialIndices;
    std::string currentmtl;

    float kd[3];
    float ks[3];
    bool kdSet;
    bool ksSet;

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

            materials.push_back(Material());
            materialIndices[mtlname] = static_cast<int>(materials.size()) - 1;

            kdSet = false;
            ksSet = false;
        }
        else if (command == "Kd")
        {
            mtlfile >> kd[0] >> kd[1] >> kd[2];

            if (ksSet)
            {
                auto scattering0 = kd[0] / (kd[0] + ks[0]);
                auto scattering1 = kd[1] / (kd[1] + ks[1]);
                auto scattering2 = kd[2] / (kd[2] + ks[2]);

                materials[materialIndices[currentmtl]].scattering = (scattering0 + scattering1 + scattering2) / 3.0f;

                for (auto i = 0; i < 3; ++i)
                    materials[materialIndices[currentmtl]].absorption[i] = 1.0f - (ks[i] + kd[i]) / 2.0f;
            }
            else
            {
                // Assume perfectly diffuse surfaces.
                materials[materialIndices[currentmtl]].scattering = 1.0f;

                for (auto i = 0; i < 3; ++i)
                    materials[materialIndices[currentmtl]].absorption[i] = .0f;
            }

            // Transmission material is kept at default (full loss).
            kdSet = true;
        }
        else if (command == "Ks")
        {
            mtlfile >> ks[0] >> ks[1] >> ks[2];

            if (kdSet)
            {
                auto scattering0 = kd[0] / (kd[0] + ks[0]);
                auto scattering1 = kd[1] / (kd[1] + ks[1]);
                auto scattering2 = kd[2] / (kd[2] + ks[2]);

                materials[materialIndices[currentmtl]].scattering = (scattering0 + scattering1 + scattering2) / 3.0f;

                for (auto i = 0; i < 3; ++i)
                    materials[materialIndices[currentmtl]].absorption[i] = 1.0f - (ks[i] + kd[i]) / 2.0f;
            }
            else
            {
                // Assume perfectly specular surfaces.
                materials[materialIndices[currentmtl]].scattering = .0f;

                for (auto i = 0; i < 3; ++i)
                    materials[materialIndices[currentmtl]].absorption[i] = .0f;
            }

            // Transmission material is kept at default (full loss).
            ksSet = true;
        }
    }

    std::ifstream file((std::string{"../../data/meshes/"} +fileName).c_str());
    if (!file.good())
    {
        printf("unable to find obj file.");
    }

    std::vector<Vector3f> vertices;
    std::vector<int> triangles;
    std::vector<int> materialIndicesForTriangles;

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
            vertices.push_back(Vector3f(x, y, z));
        }
        else if (command == "f")
        {
            std::string s[3];
            file >> s[0] >> s[1] >> s[2];

            for (auto i = 0; i < 3; ++i)
                triangles.push_back(atoi(s[i].substr(0, s[i].find_first_of("/")).c_str()) - 1);

            if (materialIndices.find(currentmtl) == materialIndices.end())
            {
                printf("WARNING: No material assigned!\n");
            }
            else
            {
                materialIndicesForTriangles.push_back(materialIndices[currentmtl]);
            }
        }
        else if (command == "usemtl")
        {
            file >> currentmtl;
        }
    }

    auto scene = ipl::shared_ptr<IScene>(SceneFactory::create(sceneType, closestHit, anyHit, batchedClosestHit, batchedAnyHit, userData, embree, radeonRays));

    auto staticMesh = scene->createStaticMesh(static_cast<int>(vertices.size()), static_cast<int>(triangles.size()) / 3, static_cast<int>(materials.size()),
                                              vertices.data(), (Triangle*) triangles.data(), materialIndicesForTriangles.data(), materials.data());

    scene->addStaticMesh(staticMesh);
    scene->commit();

    return std::move(scene);
}

static std::wstring s2ws(const std::string& str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.from_bytes(str);
}

static std::string ws2s(const std::wstring& wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}

static const std::string kMeshDirectory{"../../data/meshes/"};

std::vector<std::string> listMeshFileNames(const std::string& subdirectory)
{
    std::vector<std::string> meshFileNames;
    auto searchDirectory = kMeshDirectory + subdirectory;

    auto findData = WIN32_FIND_DATAA{};
    auto findHandle = FindFirstFileA((searchDirectory + "*.obj").c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE)
    {
        printf("WARNING: No meshes found when searching: %s.\n", searchDirectory.c_str());
        return meshFileNames;
    }

    do
    {
        meshFileNames.push_back(findData.cFileName);
        meshFileNames.back() = meshFileNames.back().substr(0, meshFileNames.back().length() - 4);
    }
    while (FindNextFileA(findHandle, &findData));

    FindClose(findHandle);

    return meshFileNames;
}
