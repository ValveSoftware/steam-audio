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

#include "probe_generator.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// ProbeGenerator
// ---------------------------------------------------------------------------------------------------------------------

const float ProbeGenerator::kDownwardOffset = 0.01f;

void ProbeGenerator::generateProbes(const IScene& scene,
                                    const Matrix4x4f& obbTransform,
                                    ProbeGenerationType type,
                                    float spacing,
                                    float height,
                                    ProbeArray& probes)
{
    switch (type)
    {
    case ProbeGenerationType::Centroid:
        generateCentroidProbe(scene, obbTransform, probes);
        break;

    case ProbeGenerationType::UniformFloor:
        generateUniformFloorProbes(scene, obbTransform, spacing, height, probes);
        break;

    default:
        throw Exception(Status::Initialization);
    }
}

void ProbeGenerator::generateCentroidProbe(const IScene& scene,
                                           const Matrix4x4f& obbTransform,
                                           ProbeArray& probes)
{
    Probe probe;

    probe.influence.center = Vector3f(obbTransform(0, 3), obbTransform(1, 3), obbTransform(2, 3));

    auto sx = Vector3f(obbTransform(0, 0), obbTransform(1, 0), obbTransform(2, 0)).length();
    auto sy = Vector3f(obbTransform(0, 1), obbTransform(1, 1), obbTransform(2, 1)).length();
    auto sz = Vector3f(obbTransform(0, 2), obbTransform(1, 2), obbTransform(2, 2)).length();
    probe.influence.radius = std::min({ sx / 2.0f, sy / 2.0f, sz / 2.0f });

    probes.probes.resize(1);
    probes.probes[0] = probe;
}

void ProbeGenerator::generateUniformFloorProbes(const IScene& scene,
                                                const Matrix4x4f& obbTransform,
                                                float spacing,
                                                float height,
                                                ProbeArray& probes)
{
    auto sx = Vector3f(obbTransform(0, 0), obbTransform(1, 0), obbTransform(2, 0)).length();
    auto sy = Vector3f(obbTransform(0, 1), obbTransform(1, 1), obbTransform(2, 1)).length();
    auto sz = Vector3f(obbTransform(0, 2), obbTransform(1, 2), obbTransform(2, 2)).length();

    if (sx < std::numeric_limits<float>::min() ||
        sy < std::numeric_limits<float>::min() ||
        sz < std::numeric_limits<float>::min())
    {
        return;
    }

    auto numProbesX = static_cast<int>(floorf(sx / spacing)) + 1;
    auto numProbesZ = static_cast<int>(floorf(sz / spacing)) + 1;
    auto residualX = (sx - (numProbesX - 1) * spacing) / 2;
    auto residualZ = (sz - (numProbesZ - 1) * spacing) / 2;

    auto downVector4f = Vector4f(obbTransform * Vector4f(0, -1, 0, 0));
    auto downVector = Vector3f::unitVector(Vector3f(downVector4f.x(), downVector4f.y(), downVector4f.z()));

    vector<Probe> _probes;

    for (auto i = 0; i < numProbesX; ++i)
    {
        for (auto j = 0; j < numProbesZ; ++j)
        {
            auto xPos = -.5f + (i * spacing + residualX) / sx;
            auto yPos = .5f;
            auto zPos = -.5f + (j * spacing + residualZ) / sz;

            auto probePoint4f = Vector4f(obbTransform * Vector4f(xPos, yPos, zPos, 1));
            auto probePoint = Vector3f(probePoint4f.x(), probePoint4f.y(), probePoint4f.z());

            computeFloorProbesBelow(scene, probePoint, downVector, obbTransform, spacing, height, _probes);
        }
    }

    probes.probes.resize(_probes.size());
    memcpy(probes.probes.data(), _probes.data(), _probes.size() * sizeof(Probe));
}

void ProbeGenerator::computeFloorProbesBelow(const IScene& scene,
                                             const Vector3f& origin,
                                             const Vector3f& downVector,
                                             const Matrix4x4f& obbTransform,
                                             float spacing,
                                             float height,
                                             vector<Probe>& probes)
{
    auto currentOrigin = origin;

    auto sy = Vector3f(obbTransform(0, 1), obbTransform(1, 1), obbTransform(2, 1)).length();
    auto distanceFromFloor = sy;

    while (distanceFromFloor > .0f)
    {
        Ray downwardRay{ currentOrigin, downVector };

        auto floorHit = scene.closestHit(downwardRay, height, distanceFromFloor + height);
        if (!floorHit.isValid())
            break;

        // Raise hit point by mHeightAboveFloor.
        auto probeLocalPos = (currentOrigin + downVector * (floorHit.distance - height));

        Probe probe;
        probe.influence.center = probeLocalPos;
        probe.influence.radius = spacing;

        probes.push_back(probe);

        // Move origin slightly downward to continue search.
        currentOrigin += downVector * (floorHit.distance + kDownwardOffset);
        distanceFromFloor -= (floorHit.distance + kDownwardOffset);
    }
}

}
