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

        // The volume correction factor in dB.
        public float volume = 0.0f;

        // Volume normalization type.
        public HRTFNormType normType = HRTFNormType.None;
    }
}
