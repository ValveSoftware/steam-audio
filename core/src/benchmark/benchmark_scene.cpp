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

#include <profiler.h>
#include <containers.h>
#include <vector.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

void BenchmarkSceneFinalizeForSceneType(IPLSceneType sceneType, const std::string& sceneTypeName)
{
    std::vector<float>   vertices;
    std::vector<int32_t> triangleIndices;
    std::vector<int>     materialIndices;

    LoadObj("../../data/meshes/sponza.obj", vertices, triangleIndices, materialIndices);
    IPLMaterial material = { 0.1f, 0.1f, 0.1f, 0.5f, 1.0f, 1.0f, 1.0f };

    IPLContext context = nullptr;
    IPLContextSettings contextSettings{ STEAMAUDIO_VERSION, nullptr, nullptr, nullptr, IPL_SIMDLEVEL_AVX512 };
    iplContextCreate(&contextSettings, &context);

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    IPLEmbreeDevice embreeDevice = nullptr;
    if (sceneType == IPL_SCENETYPE_EMBREE)
    {
        IPLEmbreeDeviceSettings embreeSettings{};
        iplEmbreeDeviceCreate(context, &embreeSettings, &embreeDevice);
    }
#endif

#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
    IPLOpenCLDevice clDevice = nullptr;
    IPLRadeonRaysDevice rrDevice = nullptr;
    if (sceneType == IPL_SCENETYPE_RADEONRAYS)
    {
        IPLOpenCLDeviceList deviceList = nullptr;
        IPLOpenCLDeviceSettings openCLSettings{ IPL_OPENCLDEVICETYPE_ANY, 8, 1.0f };
        iplOpenCLDeviceListCreate(context, &openCLSettings, &deviceList);

        iplOpenCLDeviceCreate(context, deviceList, 0, &clDevice);

        iplOpenCLDeviceListRelease(&deviceList);

        IPLRadeonRaysDeviceSettings rrSettings{};
        iplRadeonRaysDeviceCreate(clDevice, &rrSettings, &rrDevice);
    }
#endif

    IPLSceneSettings sceneSettings{};
    sceneSettings.type = sceneType;
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    sceneSettings.embreeDevice = embreeDevice;
#endif
#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
    sceneSettings.radeonRaysDevice = rrDevice;
#endif

    IPLScene scene = nullptr;
    iplSceneCreate(context, &sceneSettings, &scene);

    Timer timer;
    timer.start();

    IPLStaticMeshSettings staticMeshSettings{};
    staticMeshSettings.numVertices = static_cast<int>(vertices.size() / 3);
    staticMeshSettings.numTriangles = static_cast<int>(triangleIndices.size() / 3);
    staticMeshSettings.numMaterials = 1;
    staticMeshSettings.vertices = (IPLVector3*) vertices.data();
    staticMeshSettings.triangles = (IPLTriangle*) triangleIndices.data();
    staticMeshSettings.materialIndices = materialIndices.data();
    staticMeshSettings.materials = &material;

    IPLStaticMesh staticMesh = nullptr;
    iplStaticMeshCreate(scene, &staticMeshSettings, &staticMesh);

    auto timeElapsed = timer.elapsedMilliseconds();
    PrintOutput("%-20s %-20s %8.1f ms\n", "Sponza", sceneTypeName.c_str(), timeElapsed);

    iplStaticMeshRelease(&staticMesh);
    iplSceneRelease(&scene);
#if defined(IPL_USES_OPENCL) && defined(IPL_USES_RADEONRAYS)
    iplRadeonRaysDeviceRelease(&rrDevice);
    iplOpenCLDeviceRelease(&clDevice);
#endif
#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    iplEmbreeDeviceRelease(&embreeDevice);
#endif
    iplContextRelease(&context);
}

BENCHMARK(scene)
{
    PrintOutput("Running benchmark: Scene Finalization...\n");
    BenchmarkSceneFinalizeForSceneType(IPL_SCENETYPE_DEFAULT, "Phonon");

#if defined(IPL_USES_EMBREE) && (defined(IPL_CPU_X86) || defined(IPL_CPU_X64))
    BenchmarkSceneFinalizeForSceneType(IPL_SCENETYPE_EMBREE, "Embree");
#endif

#if defined(IPL_USES_RADEONRAYS)
    BenchmarkSceneFinalizeForSceneType(IPL_SCENETYPE_RADEONRAYS, "Radeon Rays");
#endif
    PrintOutput("\n");
}
