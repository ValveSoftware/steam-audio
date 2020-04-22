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
            var error = Error.None;

            SceneType sceneType = SceneType.Phonon;    // Scene type should always be Phonon when exporting.

            var objects = SceneExporter.GetStaticGameObjectsForExport(SceneManager.GetActiveScene());

            Vector3[] vertices = null;
            Triangle[] triangles = null;
            int[] materialIndices = null;
            Material[] materials = null;
            SceneExporter.GetGeometryAndMaterialBuffers(objects, ref vertices, ref triangles, ref materialIndices,
                ref materials, false, exportOBJ);

            if (vertices.Length == 0 || triangles.Length == 0 || materialIndices.Length == 0 || materials.Length == 0) 
            {
                throw new Exception(
                    "No Steam Audio Geometry tagged. Attach Steam Audio Geometry to one or more GameObjects that " +
                    "contain Mesh or Terrain geometry.");
            }

            error = PhononCore.iplCreateScene(globalContext, computeDevice.GetDevice(), sceneType,
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
                PhononCore.iplSaveSceneAsObj(scene, Common.ConvertString(ObjFileName()));
                Debug.Log("Scene exported to " + ObjFileName() + ".");
            }
            else
            {
                var dataSize = PhononCore.iplSaveScene(scene, null);
                var data = new byte[dataSize];
                PhononCore.iplSaveScene(scene, data);

                var fileName = SceneFileName();
                File.WriteAllBytes(fileName, data);

                Debug.Log("Scene exported to " + fileName + ".");
            }

            PhononCore.iplDestroyStaticMesh(ref staticMesh);
            PhononCore.iplDestroyScene(ref scene);
            return error;
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
                string fileName = SceneFileName();
                if (!File.Exists(fileName)) {
                    return Error.Fail;
                }

                byte[] data = File.ReadAllBytes(fileName);

                var error = PhononCore.iplLoadScene(globalContext, simulationSettings.sceneType, data, data.Length,
                    computeDevice.GetDevice(), null, ref scene);

                return error;
            }
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

        static string SceneFileName()
        {
            var fileName = Path.GetFileNameWithoutExtension(SceneManager.GetActiveScene().name) + ".phononscene";
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

        static string ObjFileName()
        {
            var fileName = Path.GetFileNameWithoutExtension(SceneManager.GetActiveScene().name) + ".obj";
            return Path.Combine(Application.streamingAssetsPath, fileName);
        }

        IntPtr scene = IntPtr.Zero;
    }
}
