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
#include "serialized_object.h"
#include "sphere.h"

namespace ipl {

// ---------------------------------------------------------------------------------------------------------------------
// BakedDataIdentifier
// ---------------------------------------------------------------------------------------------------------------------

enum class BakedDataVariation
{
    Reverb,
    StaticSource,
    StaticListener,
    Dynamic
};

enum class BakedDataType
{
    Reflections,
    Pathing
};

struct BakedDataIdentifier
{
    BakedDataType type;
    BakedDataVariation variation;
    Sphere endpointInfluence;

    bool operator<(const BakedDataIdentifier& other) const
    {
        if (variation != other.variation)
            return static_cast<int>(variation) < static_cast<int>(other.variation);

        if (type != other.type)
            return static_cast<int>(type) < static_cast<int>(other.type);

        if (endpointInfluence.center.x() != other.endpointInfluence.center.x())
            return endpointInfluence.center.x() < other.endpointInfluence.center.x();

        if (endpointInfluence.center.y() != other.endpointInfluence.center.y())
            return endpointInfluence.center.y() < other.endpointInfluence.center.y();

        if (endpointInfluence.center.z() != other.endpointInfluence.center.z())
            return endpointInfluence.center.z() < other.endpointInfluence.center.z();

        if (endpointInfluence.radius != other.endpointInfluence.radius)
            return endpointInfluence.radius < other.endpointInfluence.radius;

        return false;
    }
};

inline bool operator==(const BakedDataIdentifier& lhs,
                       const BakedDataIdentifier& rhs)
{
    return (lhs.variation == rhs.variation &&
            lhs.type == rhs.type &&
            lhs.endpointInfluence.center == rhs.endpointInfluence.center &&
            lhs.endpointInfluence.radius == rhs.endpointInfluence.radius);
}

inline bool operator!=(const BakedDataIdentifier& lhs,
                       const BakedDataIdentifier& rhs)
{
    return !(lhs == rhs);
}


// ---------------------------------------------------------------------------------------------------------------------
// IBakedData
// ---------------------------------------------------------------------------------------------------------------------

class IBakedData
{
public:
    virtual ~IBakedData()
    {}

    virtual void updateProbePosition(int index,
                                     const Vector3f& position) = 0;

    virtual void addProbe(const Sphere& influence) = 0;

    virtual void removeProbe(int index) = 0;

    virtual void updateEndpoint(const BakedDataIdentifier& identifier,
                                const Probe* probes,
                                const Sphere& endpointInfluence) = 0;

    virtual uint64_t serializedSize() const = 0;
};

}
