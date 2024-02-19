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

#include "energy_field_factory.h"
#include "opencl_energy_field.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EnergyFieldFactory
// --------------------------------------------------------------------------------------------------------------------

unique_ptr<EnergyField> EnergyFieldFactory::create(SceneType sceneType,
                                                   float duration,
                                                   int order,
                                                   shared_ptr<OpenCLDevice> openCL)
{
#if defined(IPL_USES_RADEONRAYS)
    if (sceneType == SceneType::RadeonRays)
        return ipl::make_unique<OpenCLEnergyField>(openCL, duration, order);
#endif

    return ipl::make_unique<EnergyField>(duration, order);
}

}