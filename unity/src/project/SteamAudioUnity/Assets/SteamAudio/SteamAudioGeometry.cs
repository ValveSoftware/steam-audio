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
        [Range(0, 10)]
        public int TerrainSimplificationLevel = 0;      // Simplification level for terrain mesh.
        public bool exportAllChildren;                  // Export all children with Mesh Filter. Children with Terrain are excluded.

        public int GetNumVertices()
        {
            if (exportAllChildren) {
                var objects = SceneExporter.GetGameObjectsForExport(this.gameObject);
                var numVertices = 0;
                foreach (var obj in objects) {
                    numVertices += SceneExporter.GetNumVertices(obj);
                }
                return numVertices;
            } else {
                return SceneExporter.GetNumVertices(this.gameObject);
            }
        }

        public int GetNumTriangles()
        {
            if (exportAllChildren) {
                var objects = SceneExporter.GetGameObjectsForExport(this.gameObject);
                var numTriangles = 0;
                foreach (var obj in objects) {
                    numTriangles += SceneExporter.GetNumTriangles(obj);
                }
                return numTriangles;
            } else {
                return SceneExporter.GetNumTriangles(this.gameObject);
            }
        }
    }
}