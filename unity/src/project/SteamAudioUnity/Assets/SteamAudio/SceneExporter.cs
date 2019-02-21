//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System.Collections.Generic;
using UnityEngine;

namespace SteamAudio
{
    public static class SceneExporter
    {
        static bool IsDynamicSubObject(GameObject root, GameObject obj)
        {
            return (root.GetComponentInParent<SteamAudioDynamicObject>() !=
                obj.GetComponentInParent<SteamAudioDynamicObject>());
        }

        public static List<GameObject> GetGameObjectsForExport(GameObject root, bool exportingStaticObjects = false)
        {
            var gameObjects = new List<GameObject>();

            if (exportingStaticObjects && root.GetComponentInParent<SteamAudioDynamicObject>() != null) {
                return new List<GameObject>();
            }

            var geometries = root.GetComponentsInChildren<SteamAudioGeometry>();
            foreach (var geometry in geometries) {
                if (IsDynamicSubObject(root, geometry.gameObject)) {
                    continue;
                }

                if (geometry.exportAllChildren) {
                    var meshes = geometry.GetComponentsInChildren<MeshFilter>();
                    foreach (var mesh in meshes) {
                        if (!IsDynamicSubObject(root, mesh.gameObject)) {
                            gameObjects.Add(mesh.gameObject);
                        }
                    }

                    var terrains = geometry.GetComponentsInChildren<Terrain>();
                    foreach (var terrain in terrains) {
                        if (!IsDynamicSubObject(root, terrain.gameObject)) {
                            gameObjects.Add(terrain.gameObject);
                        }
                    }
                } else {
                    // todo: what if this object has no mesh or terrain?
                    gameObjects.Add(geometry.gameObject);
                }
            }

            var uniqueGameObjects = new HashSet<GameObject>(gameObjects);

            gameObjects.Clear();
            foreach (var uniqueGameObject in uniqueGameObjects) {
                gameObjects.Add(uniqueGameObject);
            }

            return gameObjects;
        }

        public static GameObject[] GetStaticGameObjectsForExport(UnityEngine.SceneManagement.Scene scene)
        {
            var gameObjects = new List<GameObject>();

            var roots = scene.GetRootGameObjects();
            foreach (var root in roots) {
                gameObjects.AddRange(GetGameObjectsForExport(root, true));
            }

            return gameObjects.ToArray();
        }

        public static GameObject[] GetDynamicGameObjectsForExport(SteamAudioDynamicObject dynamicObject)
        {
            return GetGameObjectsForExport(dynamicObject.gameObject).ToArray();
        }

        static MaterialValue GetMaterialForGameObject(GameObject gameObject)
        {
            // todo: report error if there's a dynamic object component between gameobject and material.
            var steamAudioMaterial = gameObject.GetComponentInParent<SteamAudioMaterial>();
            if (steamAudioMaterial == null) {
                return SteamAudioManager.GetSingleton().materialValue;
            } else {
                return steamAudioMaterial.Value;
            }
        }

        static Material MaterialFromSteamAudioMaterial(MaterialValue steamAudioMaterial)
        {
            var material = new Material();
            material.absorptionLow = steamAudioMaterial.LowFreqAbsorption;
            material.absorptionMid = steamAudioMaterial.MidFreqAbsorption;
            material.absorptionHigh = steamAudioMaterial.HighFreqAbsorption;
            material.scattering = steamAudioMaterial.Scattering;
            material.transmissionLow = steamAudioMaterial.LowFreqTransmission;
            material.transmissionMid = steamAudioMaterial.MidFreqTransmission;
            material.transmissionHigh = steamAudioMaterial.HighFreqTransmission;
            return material;
        }

        static void GetMaterialMapping(GameObject[] gameObjects, ref Material[] materials, ref int[] materialIndices)
        {
            var materialMapping = new Dictionary<MaterialValue, List<int>>();

            for (var i = 0; i < gameObjects.Length; ++i) {
                var material = GetMaterialForGameObject(gameObjects[i]);
                if (!materialMapping.ContainsKey(material)) {
                    materialMapping.Add(material, new List<int>());
                }
                materialMapping[material].Add(i);
            }

            materials = new Material[materialMapping.Keys.Count];
            materialIndices = new int[gameObjects.Length];

            var index = 0;
            foreach (var material in materialMapping.Keys) {
                materials[index] = MaterialFromSteamAudioMaterial(material);
                foreach (var gameObjectIndex in materialMapping[material]) {
                    materialIndices[gameObjectIndex] = index;
                }
                ++index;
            }
        }

        static float GetTerrainSimplificationLevel(Terrain terrain)
        {
            return terrain.GetComponentInParent<SteamAudioGeometry>().TerrainSimplificationLevel;
        }

        public static int GetNumVertices(GameObject gameObject)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null) {
                return mesh.sharedMesh.vertexCount;
            } else if (terrain != null) {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapWidth;
                var h = terrain.terrainData.heightmapHeight;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));

                if (s == 0) {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                return (w * h);
            } else {
                return 0;
            }
        }

        public static int GetNumTriangles(GameObject gameObject)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null) {
                return mesh.sharedMesh.triangles.Length / 3;
            } else if (terrain != null) {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapWidth;
                var h = terrain.terrainData.heightmapHeight;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));

                if (s == 0) {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                return ((w - 1) * (h - 1) * 2);
            } else {
                return 0;
            }
        }

        static void GetVertices(GameObject gameObject, Vector3[] vertices, int offset, Transform transform)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null) {
                var vertexArray = mesh.sharedMesh.vertices;
                for (var i = 0; i < vertexArray.Length; ++i) {
                    var transformedVertex = mesh.transform.TransformPoint(vertexArray[i]);
                    if (transform != null) {
                        transformedVertex = transform.InverseTransformPoint(transformedVertex);
                    }
                    vertices[offset + i] = Common.ConvertVector(transformedVertex);
                }
            } else if (terrain != null) {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapWidth;
                var h = terrain.terrainData.heightmapHeight;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));
                if (s == 0) {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                var position = terrain.transform.position;
                var heights = terrain.terrainData.GetHeights(0, 0, terrain.terrainData.heightmapWidth,
                    terrain.terrainData.heightmapHeight);

                var index = 0;
                for (var v = 0; v < terrain.terrainData.heightmapHeight; v += s) {
                    for (var u = 0; u < terrain.terrainData.heightmapWidth; u += s) {
                        var height = heights[v, u];

                        var x = position.x + (((float)u / terrain.terrainData.heightmapWidth) *
                            terrain.terrainData.size.x);
                        var y = position.y + (height * terrain.terrainData.size.y);
                        var z = position.z + (((float)v / terrain.terrainData.heightmapHeight) *
                            terrain.terrainData.size.z);

                        var vertex = new UnityEngine.Vector3 { x = x, y = y, z = z };
                        var transformedVertex = terrain.transform.TransformPoint(vertex);
                        if (transform != null) {
                            transformedVertex = transform.InverseTransformPoint(transformedVertex);
                        }
                        vertices[offset + index] = Common.ConvertVector(transformedVertex);
                        ++index;
                    }
                }
            }
        }

        static void GetTriangles(GameObject gameObject, Triangle[] triangles, int offset)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null) {
                var triangleArray = mesh.sharedMesh.triangles;
                for (var i = 0; i < triangleArray.Length / 3; ++i) {
                    triangles[offset + i].index0 = triangleArray[3 * i + 0];
                    triangles[offset + i].index1 = triangleArray[3 * i + 1];
                    triangles[offset + i].index2 = triangleArray[3 * i + 2];
                }
            } else if (terrain != null) {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapWidth;
                var h = terrain.terrainData.heightmapHeight;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));
                if (s == 0) {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                var index = 0;
                for (var v = 0; v < h - 1; ++v) {
                    for (var u = 0; u < w - 1; ++u) {
                        var i0 = v * w + u;
                        var i1 = (v + 1) * w + u;
                        var i2 = v * w + (u + 1);
                        triangles[offset + index] = new Triangle {
                            index0 = i0,
                            index1 = i1,
                            index2 = i2
                        };

                        i0 = v * w + (u + 1);
                        i1 = (v + 1) * w + u;
                        i2 = (v + 1) * w + (u + 1);
                        triangles[offset + index + 1] = new Triangle {
                            index0 = i0,
                            index1 = i1,
                            index2 = i2
                        };

                        index += 2;
                    }
                }
            }
        }

        static void FixupTriangleIndices(Triangle[] triangles, int startIndex, int endIndex, int indexOffset)
        {
            for (var i = startIndex; i < endIndex; ++i) {
                triangles[i].index0 += indexOffset;
                triangles[i].index1 += indexOffset;
                triangles[i].index2 += indexOffset;
            }
        }

        public static void GetGeometryAndMaterialBuffers(GameObject[] gameObjects, ref Vector3[] vertices,
            ref Triangle[] triangles, ref int[] materialIndices, ref Material[] materials, bool isDynamic,
            bool exportOBJ)
        {
            var numVertices = new int[gameObjects.Length];
            var numTriangles = new int[gameObjects.Length];
            var totalNumVertices = 0;
            var totalNumTriangles = 0;
            for (var i = 0; i < gameObjects.Length; ++i) {
                numVertices[i] = GetNumVertices(gameObjects[i]);
                numTriangles[i] = GetNumTriangles(gameObjects[i]);
                totalNumVertices += numVertices[i];
                totalNumTriangles += numTriangles[i];
            }

            int[] materialIndicesPerObject = null;
            GetMaterialMapping(gameObjects, ref materials, ref materialIndicesPerObject);

            vertices = new Vector3[totalNumVertices];
            triangles = new Triangle[totalNumTriangles];
            materialIndices = new int[totalNumTriangles];

            Transform transform = null;
            if (isDynamic && !exportOBJ) {
                var dynamicObject = gameObjects[0].GetComponent<SteamAudioDynamicObject>();
                if (dynamicObject == null) {
                    dynamicObject = gameObjects[0].GetComponentInParent<SteamAudioDynamicObject>();
                }
                transform = dynamicObject.transform;
            }

            var verticesOffset = 0;
            var trianglesOffset = 0;
            for (var i = 0; i < gameObjects.Length; ++i) {
                GetVertices(gameObjects[i], vertices, verticesOffset, transform);
                GetTriangles(gameObjects[i], triangles, trianglesOffset);
                FixupTriangleIndices(triangles, trianglesOffset, trianglesOffset + numTriangles[i], verticesOffset);

                for (int j = 0; j < numTriangles[i]; ++j) {
                    materialIndices[trianglesOffset + j] = materialIndicesPerObject[i];
                }

                verticesOffset += numVertices[i];
                trianglesOffset += numTriangles[i];
            }
        }
    }
}
