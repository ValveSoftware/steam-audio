//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
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
    }
}
