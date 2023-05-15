//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    /* 
     * Represents a serializable object that is created when importing a .sofa file as an asset.
     */
    public class SOFAFile : ScriptableObject
    {
        // The name of the sofa file that was imported.
        public string sofaName = "";

        // The imported data.
        public byte[] data = null;

        // The volume correction factor.
        public float volume = 1.0f;
    }
}
