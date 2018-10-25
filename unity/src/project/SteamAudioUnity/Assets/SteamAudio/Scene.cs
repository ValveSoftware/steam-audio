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
        public IntPtr GetScene()
        {
            return scene;
        }

        public Error Export(ComputeDevice computeDevice, SimulationSettings simulationSettings, 
            MaterialValue defaultMaterial, IntPtr globalContext, bool exportOBJ = false)
        {
            var error = Error.None;

            var objects = GameObject.FindObjectsOfType<SteamAudioGeometry>();
            var totalNumVertices = 0;
            var totalNumTriangles = 0;
            var totalNumMaterials = 1;  // Global material.

            for (var i = 0; i < objects.Length; ++i)
            {
                totalNumVertices += objects[i].GetNumVertices();
                totalNumTriangles += objects[i].GetNumTriangles();
                totalNumMaterials += objects[i].GetNumMaterials();
            }

            simulationSettings.sceneType = SceneType.Phonon;    // Scene type should always be Phonon when exporting.

            var vertices = new Vector3[totalNumVertices];
            var triangles = new Triangle[totalNumTriangles];
            var materialIndices = new int[totalNumTriangles];
            var materials = new Material[totalNumMaterials + 1];    // Offset added to avoid creating Material
                                                                    // for each object and then copying it.

            var vertexOffset = 0;
            var triangleOffset = 0;

            var materialOffset = 1;
            materials[0].absorptionHigh = defaultMaterial.HighFreqAbsorption;
            materials[0].absorptionMid = defaultMaterial.MidFreqAbsorption;
            materials[0].absorptionLow = defaultMaterial.LowFreqAbsorption;
            materials[0].scattering = defaultMaterial.Scattering;
            materials[0].transmissionHigh = defaultMaterial.HighFreqTransmission;
            materials[0].transmissionMid = defaultMaterial.MidFreqTransmission;
            materials[0].transmissionLow = defaultMaterial.LowFreqTransmission;

            for (var i = 0; i < objects.Length; ++i)
            {
                objects[i].GetGeometry(vertices, ref vertexOffset, triangles, ref triangleOffset, materials, 
                    materialIndices, ref materialOffset);
            }

            error = PhononCore.iplCreateScene(globalContext, computeDevice.GetDevice(), simulationSettings,
                materials.Length, materials, null, null, null, null, IntPtr.Zero, ref scene);
            if (error != Error.None)
            {
                throw new Exception("Unable to create scene for export (" + objects.Length.ToString() +
                    " materials): [" + error.ToString() + "]");
            }

            var staticMesh = IntPtr.Zero;
            error = PhononCore.iplCreateStaticMesh(scene, totalNumVertices, totalNumTriangles, vertices, triangles,
                materialIndices, ref staticMesh);
            if (error != Error.None)
            {
                throw new Exception("Unable to create static mesh for export (" + totalNumVertices.ToString() +
                    " vertices, " + totalNumTriangles.ToString() + " triangles): [" + error.ToString() + "]");
            }

#if UNITY_EDITOR
            if (!Directory.Exists(Application.streamingAssetsPath))
                UnityEditor.AssetDatabase.CreateFolder("Assets", "StreamingAssets");
#endif

            if (exportOBJ)
            {
                PhononCore.iplSaveSceneAsObj(scene, Common.ConvertString(ObjFileName()));
                Debug.Log("Scene dumped to " + ObjFileName() + ".");
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

        public Error Create(ComputeDevice computeDevice, SimulationSettings simulationSettings, IntPtr globalContext)
        {
            if (simulationSettings.sceneType == SceneType.Custom) {
                if (materialBuffer == IntPtr.Zero) {
                    materialBuffer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Material)));
                }

                var error = PhononCore.iplCreateScene(globalContext, computeDevice.GetDevice(), simulationSettings, 0, 
                                                      null, RayTracer.ClosestHit, RayTracer.AnyHit, null, null, 
                                                      IntPtr.Zero, ref scene);
                return error;
            } else {
                string fileName = SceneFileName();
                if (!File.Exists(fileName)) {
                    return Error.Fail;
                }

                byte[] data = File.ReadAllBytes(fileName);

                var error = PhononCore.iplLoadScene(globalContext, simulationSettings, data, data.Length,
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

        static IntPtr materialBuffer = IntPtr.Zero;

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
            var steamAudioManager = GameObject.FindObjectOfType<SteamAudioManager>();
            if (steamAudioManager == null) {
                return new LayerMask();
            }

            return steamAudioManager.layerMask;
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
