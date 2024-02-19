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

#include "impulse_response.h"
#include "indirect_effect.h"
#include "scene_factory.h"
#include "opencl_device.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ImpulseResponseFactory
// --------------------------------------------------------------------------------------------------------------------

namespace ImpulseResponseFactory
{
    unique_ptr<ImpulseResponse> create(IndirectEffectType indirectType,
                                       float duration,
                                       int order,
                                       int samplingRate,
                                       shared_ptr<OpenCLDevice> openCL);
};

}