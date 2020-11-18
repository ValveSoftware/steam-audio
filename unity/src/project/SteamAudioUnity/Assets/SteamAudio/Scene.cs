//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace SteamAudio
{
    public class Scene
    {
        static IntPtr materialBuffer = IntPtr.Zero;
        static UnityEngine.SceneManagement.Scene s_activeScene;
        static SteamAudioManager s_steamAudioManager = null;

        public IntPtr GetScene()
        {
            return scene;
        }

        public Error Export(ComputeDevice computeDevice, MaterialValue defaultMaterial, 
            IntPtr globalContext, bool exportOBJ = false)
        {
            SceneType sceneType = SceneType.Phonon;    // Scene type should always be Phonon when exporting.
            int exportedScenesCount = 0;

            for (int i = 0; i < SceneManager.sceneCount; ++i)
            {
                var unityScene = SceneManager.GetSceneAt(i);
                if (!unityScene.isLoaded)
                {
                    Debug.LogWarning("Scene " + SceneManager.GetSceneAt(i).name + " is not loaded in the hierarchy.");
                    continue;
                }

                var objects = SceneExporter.GetStaticGameObjectsForExport(unityScene);

                Vector3[] vertices = null;
                Triangle[] triangles = null;
                int[] materialIndices = null;
                Material[] materials = null;
                SceneExporter.GetGeometryAndMaterialBuffers(objects, ref vertices, ref triangles, ref materialIndices,
                    ref materials, false, exportOBJ);

                if (vertices.Length == 0 || triangles.Length == 0 || materialIndices.Length == 0 || materials.Length == 0)
                {
                    Debug.LogWarning("Scene " + unityScene.name + " has no Steam Audio geometry.");
                    continue;
                }

                ++exportedScenesCount;

                Error error = PhononCore.iplCreateScene(globalContext, computeDevice.GetDevice(), sceneType,
                    materials.Length, materials, null, null, null, null, IntPtr.Zero, ref scene);
                if (error != Error.None)
                {
                    throw new Exception("Unable to create scene for export (" + materials.Length.ToString() +
                        " materials): [" + error.ToString() + "]");
                }

                var staticMesh = IntPtr.Zero;
                error = PhononCore.iplCreateStaticMesh(scene, vertices.Length, triangles.Length, vertices, triangles,
                    materialIndices, ref staticMesh);
                if (error != Error.None)
                {
                    throw new Exception("Unable to create static mesh for export (" + vertices.Length.ToString() +
                        " vertices, " + triangles.Length.ToString() + " triangles): [" + error.ToString() + "]");
                }

#if UNITY_EDITOR
                if (!Directory.Exists(Application.streamingAssetsPath))
                    UnityEditor.AssetDatabase.CreateFolder("Assets", "StreamingAssets");
#endif

                if (exportOBJ)
                {
                    var fileName = ObjFileName(unityScene.name);
                    PhononCore.iplSaveSceneAsObj(scene, Common.ConvertString(fileName));
                    Debug.Log("Scene exported to " + fileName + ".");
                }
                else
                {
                    var dataSize = PhononCore.iplSaveScene(scene, null);
                    var data = new byte[dataSize];
                    PhononCore.iplSaveScene(scene, data);

                    var fileName = SceneFileName(unityScene.name);
                    File.WriteAllBytes(fileName, data);

                    Debug.Log("Scene exported to " + fileName + ".");
                }

                PhononCore.iplDestroyStaticMesh(ref staticMesh);
                PhononCore.iplDestroyScene(ref scene);
            }

            if (exportedScenesCount == 0)
            {
                throw new Exception(
                    "No Steam Audio Geometry tagged in the scene hierarchy. Attach Steam Audio Geometry " +
                    "to one or more GameObjects that contain Mesh or Terrain geometry.");
            }

            return Error.None;
        }

        SteamAudioGeometry[] FilterGameObjectsWithDynamicGeometry(SteamAudioGeometry[] objects)
        {
            if (objects == null || objects.Length <= 0) {
                return null;
            }

            var numExcluded = 0;
            var excluded = new bool[objects.Length];
            for (var i = 0; i < objects.Length; ++i) {
                var dynamicObjects = objects[i].GetComponentsInParent<SteamAudioDynamicObject>();
                excluded[i] = (dynamicObjects != null && dynamicObjects.Length > 0);
                if (excluded[i]) {
                    ++numExcluded;
                }
            }
            var numIncluded = objects.Length - numExcluded;

            var filteredObjects = new SteamAudioGeometry[numIncluded];
            var index = 0;
            for (var i = 0; i < objects.Length; ++i) {
                if (excluded[i]) {
                    continue;
                }
                filteredObjects[index] = objects[i];
                ++index;
            }

            return filteredObjects;
        }

        public Error Create(ComputeDevice computeDevice, SimulationSettings simulationSettings, IntPtr globalContext)
        {
            if (simulationSettings.sceneType == SceneType.Custom) {
                if (materialBuffer == IntPtr.Zero) {
                    materialBuffer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Material)));
                }

                var error = PhononCore.iplCreateScene(globalContext, computeDevice.GetDevice(), 
                    simulationSettings.sceneType, 0, null, RayTracer.ClosestHit, RayTracer.AnyHit, null, null, 
                    IntPtr.Zero, ref scene);

                return error;
            } else {
                string fileName = SceneFileName(SceneManager.GetActiveScene().name);
                if (!File.Exists(fileName)) {
                    return Error.Fail;
                }

                byte[] data = File.ReadAllBytes(fileName);

                var error = PhononCore.iplLoadScene(globalContext, simulationSettings.sceneType, data, data.Length,
                    computeDevice.GetDevice(), null, ref scene);

                return error;
            }
        }

        public Error AddAdditiveScene(UnityEngine.SceneManagement.Scene unityScene, IntPtr activeScene,
            ComputeDevice computeDevice, SimulationSettings simulationSettings, IntPtr globalContext,
            out IntPtr addedScene, out IntPtr addedMesh)
        {
            addedScene = IntPtr.Zero;
            addedMesh = IntPtr.Zero;

            if (simulationSettings.sceneType != SceneType.Embree)
            {
                Debug.LogWarning("Additive scenes are only supported with Embree.");
                return Error.Fail;
            }

            string fileName = SceneFileName(unityScene.name);
            if (!File.Exists(fileName))
            {
                return Error.Fail;
            }

            byte[] data = File.ReadAllBytes(fileName);
            var error = PhononCore.iplLoadScene(globalContext, simulationSettings.sceneType, data, data.Length,
                computeDevice.GetDevice(), null, ref addedScene);

            var transformMatrix = new Matrix4x4
            {
                m00 = 1.0f, m01 = 0.0f, m02 = 0.0f, m03 = 0.0f,
                m10 = 0.0f, m11 = 1.0f, m12 = 0.0f, m13 = 0.0f,
                m20 = 0.0f, m21 = 0.0f, m22 = 1.0f, m23 = 0.0f,
                m30 = 0.0f, m31 = 0.0f, m32 = 0.0f, m33 = 1.0f
            };

            error = PhononCore.iplCreateInstancedMesh(activeScene, addedScene, transformMatrix, ref addedMesh);
            if (error != Error.None)
            {
                return error;
            }

            PhononCore.iplAddInstancedMesh(activeScene, addedMesh);
            return error;
        }

        public void Destroy()
        {
            if (materialBuffer != IntPtr.Zero) {
                Marshal.FreeHGlobal(materialBuffer);
                materialBuffer = IntPtr.Zero;
            }

            if (scene != IntPtr.Zero)
                PhononCore.iplDestroyScene(ref scene);
        }

        static Material CopyMaterial(MaterialValue materialValue)
        {
            var material = new Material();
            material.absorptionLow    = materialValue.LowFreqAbsorption;
            material.absorptionMid    = materialValue.MidFreqAbsorption;
            material.absorptionHigh   = materialValue.HighFreqAbsorption;
            material.scattering       = materialValue.Scattering;
            material.transmissionLow  = materialValue.LowFreqTransmission;
            material.transmissionMid  = materialValue.MidFreqTransmission;
            material.transmissionHigh = materialValue.HighFreqTransmission;
            return material;
        }

        public static LayerMask GetSteamAudioLayerMask()
        {
            UnityEngine.SceneManagement.Scene activeScene = SceneManager.GetActiveScene();
            if (s_activeScene != activeScene) 
            {
                s_activeScene = activeScene;
                s_steamAudioManager = GameObject.FindObjectOfType<SteamAudioManager>();
            }
            
            if (s_steamAudioManager == null)
            {
                return new LayerMask();
            }

            return s_steamAudioManager.layerMask;
        }

        public static bool HasSteamAudioGeometry(Transform obj)
        {
            var currentObject = obj;
            while (currentObject != null) {
                var steamAudioGeometry = currentObject.GetComponent<SteamAudioGeometry>();
                if (steamAudioGeometry != null) {
                    return (currentObject == obj || steamAudioGeometry.exportAllChildren);
                }
                currentObject = currentObject.parent;
            }
            return false;
        }

        public static IntPtr GetSteamAudioMaterialBuffer(Transform obj)
        {
            var material = new Material();
            var found = false;

            var currentObject = obj;
            while (currentObject != null) {
                var steamAudioMaterial = currentObject.GetComponent<SteamAudioMaterial>();
                if (steamAudioMaterial != null) {
                    material = CopyMaterial(steamAudioMaterial.Value);
                    found = true;
                    break;
                }
                currentObject = currentObject.parent;
            }

            if (!found) {
                var steamAudioManager = GameObject.FindObjectOfType<SteamAudioManager>();
                if (steamAudioManager == null) {
                    return IntPtr.Zero;
                }
                material = CopyMaterial(steamAudioManager.materialValue);
            }

            Marshal.StructureToPtr(material, materialBuffer, true);
            return materialBuffer;
        }

        public static string SceneFileName(string sceneName)
        {
            var fileName = Path.GetFileNameWithoutExtension(sceneName) + ".phononscene";
            var streamingAssetsFileName = Path.Combine(Application.streamingAssetsPath, fileName);

#if UNITY_ANDROID && !UNITY_EDITOR
            var tempFileName = Path.Combine(Application.temporaryCachePath, fileName);

            if (File.Exists(tempFileName))
                File.Delete(tempFileName);

                try
                  {
                  var streamingAssetLoader = new WWW(streamingAssetsFileName);
                while (!streamingAssetLoader.isDone);
                        if (string.IsNullOrEmpty(streamingAssetLoader.error))
                        {
                              var assetData = streamingAssetLoader.bytes;
                              using (var dataWriter = new BinaryWriter(new FileStream(tempFileName, FileMode.Create)))
                              {
                                    dataWriter.Write(assetData);
                                    dataWriter.Close();
                              }
                        }
                        else
                        {
                              Debug.Log(streamingAssetLoader.error);
                        }
            }
            catch (Exception exception)
            {
                      Debug.LogError(exception.ToString());
            }
            return tempFileName;
#else
            return streamingAssetsFileName;
#endif
        }

        static string ObjFileName(string sceneName)
        {
            var fileName = Path.GetFileNameWithoutExtension(sceneName) + ".obj";
            return Path.Combine(Application.streamingAssetsPath, fileName);
        }

        IntPtr scene = IntPtr.Zero;
    }
}
