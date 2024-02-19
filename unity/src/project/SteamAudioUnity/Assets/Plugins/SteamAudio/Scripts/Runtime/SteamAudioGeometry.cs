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
    [AddComponentMenu("Steam Audio/Steam Audio Geometry")]
    public class SteamAudioGeometry : MonoBehaviour
    {
        [Header("Material Settings")]
        public SteamAudioMaterial material = null;
        [Header("Export Settings")]
        public bool exportAllChildren = false;
        [Header("Terrain Settings")]
        [Range(0, 10)]
        public int terrainSimplificationLevel = 0;

#if STEAMAUDIO_ENABLED
        public int GetNumVertices()
        {
            if (exportAllChildren)
            {
                var objects = SteamAudioManager.GetGameObjectsForExport(gameObject);

                var numVertices = 0;
                foreach (var obj in objects)
                {
                    numVertices += SteamAudioManager.GetNumVertices(obj);
                }

                return numVertices;
            }
            else
            {
                return SteamAudioManager.GetNumVertices(gameObject);
            }
        }

        public int GetNumTriangles()
        {
            if (exportAllChildren)
            {
                var objects = SteamAudioManager.GetGameObjectsForExport(gameObject);

                var numTriangles = 0;
                foreach (var obj in objects)
                {
                    numTriangles += SteamAudioManager.GetNumTriangles(obj);
                }

                return numTriangles;
            }
            else
            {
                return SteamAudioManager.GetNumTriangles(gameObject);
            }
        }
#endif
    }
}
