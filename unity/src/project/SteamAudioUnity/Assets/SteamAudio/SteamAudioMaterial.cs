//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{

    //
    // SteamAudioMaterial
    // A component representing a material that can be set to a preset or a custom value.
    //

    [AddComponentMenu("Steam Audio/Steam Audio Material")]
    public class SteamAudioMaterial : MonoBehaviour
    {
        //
        // Data members.
        //

        // Name of the current preset.
        public MaterialPreset Preset;

        // Current values of the material.
        public MaterialValue Value;
    }
}