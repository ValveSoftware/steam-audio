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

#include "reconstructor_factory.h"

#include "opencl_reconstructor.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ReconstructorFactory
// --------------------------------------------------------------------------------------------------------------------

unique_ptr<IReconstructor> ReconstructorFactory::create(SceneType sceneType,
                                                        IndirectEffectType indirectType,
                                                        float maxDuration,
                                                        int maxOrder,
                                                        int samplingRate,
                                                        shared_ptr<RadeonRaysDevice> radeonRays)
{
#if defined(IPL_USES_RADEONRAYS) && defined(IPL_USES_TRUEAUDIONEXT)
    if (sceneType == SceneType::RadeonRays && indirectType == IndirectEffectType::TrueAudioNext)
        return ipl::make_unique<OpenCLReconstructor>(radeonRays, maxDuration, maxOrder, samplingRate);
#endif

    return make_unique<Reconstructor>(maxDuration, maxOrder, samplingRate);
}

}