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

#pragma once

#include "probe.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// ProbeArray
// ---------------------------------------------------------------------------------------------------------------------

class ProbeArray
{
public:
    Array<Probe> probes;

    int numProbes() const
    {
        return static_cast<int>(probes.size(0));
    }

    Probe& operator[](int i)
    {
        return probes[i];
    }

    const Probe& operator[](int i) const
    {
        return probes[i];
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// ProbeGenerator
// ---------------------------------------------------------------------------------------------------------------------

enum class ProbeGenerationType
{
    Centroid,
    UniformFloor,
    Octree
};

class ProbeGenerator
{
public:
    static void generateProbes(const IScene& scene,
                               const Matrix4x4f& obbTransform,
                               ProbeGenerationType type,
                               float spacing,
                               float height,
                               ProbeArray& probes);

    static void generateCentroidProbe(const IScene& scene,
                                      const Matrix4x4f& obbTransform,
                                      ProbeArray& probes);

    static void generateUniformFloorProbes(const IScene& scene,
                                           const Matrix4x4f& obbTransform,
                                           float spacing,
                                           float height,
                                           ProbeArray& probes);

private:
    static const float kDownwardOffset;

    static void computeFloorProbesBelow(const IScene& scene,
                                        const Vector3f& origin,
                                        const Vector3f& downVector,
                                        const Matrix4x4f& obbTransform,
                                        float spacing, float height,
                                        vector<Probe>& probes);
};

}
