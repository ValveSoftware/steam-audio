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

#include "impulse_response_factory.h"
#include "opencl_impulse_response.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ImpulseResponseFactory
// --------------------------------------------------------------------------------------------------------------------

unique_ptr<ImpulseResponse> ImpulseResponseFactory::create(IndirectEffectType indirectType,
                                                           float duration,
                                                           int order,
                                                           int samplingRate,
                                                           shared_ptr<OpenCLDevice> openCL)
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    if (indirectType == IndirectEffectType::TrueAudioNext)
        return ipl::make_unique<OpenCLImpulseResponse>(openCL, duration, order, samplingRate);
#endif

    return ipl::make_unique<ImpulseResponse>(duration, order, samplingRate);
}

}