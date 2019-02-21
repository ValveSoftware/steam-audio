//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.IO;
using UnityEngine;

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Dynamic Object")]
    [RequireComponent(typeof(SteamAudioMaterial))]
    public class SteamAudioDynamicObject : MonoBehaviour
    {
        public string assetFileName = "";

        SteamAudioManager manager = null;
        IntPtr instancedMesh = IntPtr.Zero;

        void Awake()
        {
            manager = SteamAudioManager.GetSingleton();
            if (manager == null) {
                throw new Exception();
            }
            manager.Initialize(GameEngineStateInitReason.Playing);

            if (assetFileName != null && assetFileName.Length > 0) {
                manager.CreateInstancedMesh(assetFileName, transform, ref instancedMesh);
            }
        }

        void OnDestroy()
        {
            if (manager == null || instancedMesh == IntPtr.Zero) {
                return;
            }
            manager.DestroyInstancedMesh(ref instancedMesh);
        }

        void OnEnable()
        {
            if (manager == null || instancedMesh == IntPtr.Zero) {
                return;
            }
            manager.EnableInstancedMesh(instancedMesh);
        }

        void OnDisable()
        {
            if (manager == null || instancedMesh == IntPtr.Zero) {
                return;
            }
            manager.DisableInstancedMesh(instancedMesh);
        }

        void Update()
        {
            if (manager == null || instancedMesh == IntPtr.Zero) {
                return;
            }
            manager.UpdateInstancedMeshTransform(instancedMesh, transform);
        }

        public void Export(string assetFileName, bool exportOBJ)
        {
            if (!ValidateGeometry()) {
                return;
            }

            assetFileName = Application.streamingAssetsPath + "/" + assetFileName;

            var objects = SceneExporter.GetDynamicGameObjectsForExport(this);

            Vector3[] vertices = null;
            Triangle[] triangles = null;
            int[] materialIndices = null;
            Material[] materials = null;
            SceneExporter.GetGeometryAndMaterialBuffers(objects, ref vertices, ref triangles, ref materialIndices,
                ref materials, true, exportOBJ);

            // TODO: Make the log callback function accessible from any class.
            var context = IntPtr.Zero;
            var status = PhononCore.iplCreateContext(null, IntPtr.Zero, IntPtr.Zero, ref context);
            if (status != Error.None) {
                throw new Exception();
            }

            var computeDevice = IntPtr.Zero;

            var simulationSettings = new SimulationSettings {
                sceneType = SceneType.Phonon
            };

            var scene = IntPtr.Zero;
            status = PhononCore.iplCreateScene(context, computeDevice, simulationSettings, materials.Length, materials,
                null, null, null, null, IntPtr.Zero, ref scene);
            if (status != Error.None) {
                throw new Exception();
            }

            var staticMesh = IntPtr.Zero;
            status = PhononCore.iplCreateStaticMesh(scene, vertices.Length, triangles.Length, vertices, triangles,
                materialIndices, ref staticMesh);
            if (status != Error.None) {
                throw new Exception();
            }

            if (exportOBJ) {
                PhononCore.iplSaveSceneAsObj(scene, Common.ConvertString(assetFileName + ".obj"));
            } else {
                var size = PhononCore.iplSaveScene(scene, null);
                var data = new byte[size];
                PhononCore.iplSaveScene(scene, data);

                File.WriteAllBytes(assetFileName, data);
            }

            PhononCore.iplDestroyStaticMesh(ref staticMesh);
            PhononCore.iplDestroyScene(ref scene);
            PhononCore.iplDestroyContext(ref context);

            var exportedFileName = (exportOBJ) ? assetFileName + ".obj" : assetFileName;
            Debug.Log(string.Format("Steam Audio Dynamic Object [{0}]: Exported to {1}.", name, exportedFileName));
        }

        bool ValidateGeometry()
        {
            var geometryComponents = GetComponentsInChildren<SteamAudioGeometry>();
            if (geometryComponents == null || geometryComponents.Length == 0) {
                ReportValidationFailure("No Steam Audio Geometry components attached.");
                return false;
            }

            return true;
        }

        void ReportValidationFailure(string message)
        {
            Debug.LogError(string.Format("Steam Audio Dynamic Object [{0}]: Validation failed: {1}", name, message));
        }
    }
}
