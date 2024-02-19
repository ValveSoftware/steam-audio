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

#include "error.h"
#include "hrtf_map.h"
#include "hrtf_map_factory.h"
#if !defined(IPL_DISABLE_SOFA)
#include "sofa_hrtf_map.h"
#endif

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HRTFMapFactory
// --------------------------------------------------------------------------------------------------------------------

unique_ptr<IHRTFMap> HRTFMapFactory::create(const HRTFSettings& hrtfSettings,
                                            int samplingRate)
{
    switch (hrtfSettings.type)
    {
    case HRTFMapType::Default:
        return make_unique<HRTFMap>(samplingRate, hrtfSettings.hrtfData);

#if !defined(IPL_DISABLE_SOFA)
    case HRTFMapType::SOFA:
        return make_unique<SOFAHRTFMap>(hrtfSettings, samplingRate);
#endif

    default:
        throw Exception(Status::Initialization);
    }
}

}
