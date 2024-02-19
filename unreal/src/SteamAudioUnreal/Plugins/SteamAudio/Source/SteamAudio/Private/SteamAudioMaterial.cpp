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

#include "SteamAudioMaterial.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioMaterial
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioMaterial::USteamAudioMaterial()
    : AbsorptionLow(0.1f)
    , AbsorptionMid(0.1f)
    , AbsorptionHigh(0.1f)
    , Scattering(0.5f)
    , TransmissionLow(0.1f)
    , TransmissionMid(0.1f)
    , TransmissionHigh(0.1f)
{}
