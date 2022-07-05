//
// Copyright (C) Valve Corporation. All rights reserved.
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
