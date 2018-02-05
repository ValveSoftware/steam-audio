//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace SteamAudio
{
    //
    // ProbeBox
    // Represents a Phonon Box.
    //

    [AddComponentMenu("Steam Audio/Steam Audio Probe Box")]
    public class SteamAudioProbeBox : MonoBehaviour
    {
        void OnDrawGizmosSelected()
        {
            Color oldColor = Gizmos.color;
            Gizmos.color = Color.magenta;

            Matrix4x4 oldMatrix = Gizmos.matrix;
            Gizmos.matrix = transform.localToWorldMatrix;
            Gizmos.DrawWireCube(new UnityEngine.Vector3(0, 0, 0), new UnityEngine.Vector3(1, 1, 1));
            Gizmos.matrix = oldMatrix;

            float PROBE_DRAW_SIZE = .1f;
            Gizmos.color = Color.yellow;
            if (probeSpherePoints != null)
                for (int i = 0; i < probeSpherePoints.Length / 3; ++i)
                {
                    UnityEngine.Vector3 center = new UnityEngine.Vector3(probeSpherePoints[3 * i + 0], 
                        probeSpherePoints[3 * i + 1], -probeSpherePoints[3 * i + 2]);
                    Gizmos.DrawCube(center, new UnityEngine.Vector3(PROBE_DRAW_SIZE, PROBE_DRAW_SIZE, PROBE_DRAW_SIZE));
                }
            Gizmos.color = oldColor;
        }

        public void GenerateProbes()
        {
            ProbePlacementParameters placementParameters;
            placementParameters.placement = placementStrategy;
            placementParameters.maxNumTriangles = maxNumTriangles;
            placementParameters.maxOctreeDepth = maxOctreeDepth;
            placementParameters.horizontalSpacing = horizontalSpacing;
            placementParameters.heightAboveFloor = heightAboveFloor;

            // Initialize environment
            SteamAudioManager steamAudioManager = null;
            try
            {
                steamAudioManager = FindObjectOfType<SteamAudioManager>();
                if (steamAudioManager == null)
                    throw new Exception("Phonon Manager Settings object not found in the scene! Click Window > Phonon");

                steamAudioManager.Initialize(GameEngineStateInitReason.GeneratingProbes);

                if (steamAudioManager.GameEngineState().Scene().GetScene() == IntPtr.Zero)
                {
                    Debug.LogError("Scene not found. Make sure to pre-export the scene.");
                    steamAudioManager.Destroy();
                    return;
                }
            }
            catch (Exception e)
            {
                Debug.LogError(e.Message);
                return;
            }

            // Create bounding box for the probe.
            IntPtr probeBox = IntPtr.Zero;
            PhononCore.iplCreateProbeBox(steamAudioManager.GameEngineState().Context(), steamAudioManager.GameEngineState().Scene().GetScene(),
            Common.ConvertMatrix(gameObject.transform.localToWorldMatrix), placementParameters, null, ref probeBox);

            int numProbes = PhononCore.iplGetProbeSpheres(probeBox, null);
            probeSpherePoints = new float[3*numProbes];
            probeSphereRadii = new float[numProbes];

            Sphere[] probeSpheres = new Sphere[numProbes];
            PhononCore.iplGetProbeSpheres(probeBox, probeSpheres);
            for (int i = 0; i < numProbes; ++i)
            {
                probeSpherePoints[3 * i + 0] = probeSpheres[i].centerx;
                probeSpherePoints[3 * i + 1] = probeSpheres[i].centery;
                probeSpherePoints[3 * i + 2] = probeSpheres[i].centerz;
                probeSphereRadii[i] = probeSpheres[i].radius;
            }

            // Save probe box into searlized data;
            int probeBoxSize = PhononCore.iplSaveProbeBox(probeBox, null);
            var probeBoxData = new byte[probeBoxSize];
            PhononCore.iplSaveProbeBox(probeBox, probeBoxData);
            SaveData(probeBoxData);

            if (steamAudioManager.GameEngineState().Scene().GetScene() != IntPtr.Zero)
                Debug.Log("Generated " + probeSpheres.Length + " probes for game object " + gameObject.name + ".");

            // Cleanup.
            PhononCore.iplDestroyProbeBox(ref probeBox);
            steamAudioManager.Destroy();
            ResetLayers();

            // Redraw scene view for probes to show up instantly.
#if UNITY_EDITOR
            UnityEditor.SceneView.RepaintAll();
#endif
        }

        public void DeleteBakedDataByIdentifier(BakedDataIdentifier identifier)
        {
            SteamAudioManager steamAudioManager = null;
            IntPtr probeBox = IntPtr.Zero;
            try
            {
                steamAudioManager = FindObjectOfType<SteamAudioManager>();
                if (steamAudioManager == null)
                    throw new Exception("Phonon Manager Settings object not found in the scene! Click Window > Phonon");

                steamAudioManager.Initialize(GameEngineStateInitReason.EditingProbes);
                var context = steamAudioManager.GameEngineState().Context();

                byte[] probeBoxData = LoadData();
                PhononCore.iplLoadProbeBox(context, probeBoxData, probeBoxData.Length, ref probeBox);
                PhononCore.iplDeleteBakedDataByIdentifier(probeBox, identifier);
                RemoveLayer(identifier);

                int probeBoxSize = PhononCore.iplSaveProbeBox(probeBox, null);
                probeBoxData = new byte[probeBoxSize];
                PhononCore.iplSaveProbeBox(probeBox, probeBoxData);
                SaveData(probeBoxData);

                steamAudioManager.Destroy();
            }
            catch (Exception e)
            {
                Debug.LogError(e.Message);
            }

        }

        // Public members.
        public ProbePlacementStrategy placementStrategy;

        [Range(.1f, 50f)]
        public float horizontalSpacing = 5f;

        [Range(.1f, 20f)]
        public float heightAboveFloor = 1.5f;

        [Range(1, 1024)]
        public int maxNumTriangles = 64;

        [Range(1, 10)]
        public int maxOctreeDepth = 2;

        public int dataSize = 0;
        public float[] probeSpherePoints = null;
        public float[] probeSphereRadii = null;

        string cachedDataFileName = "";

        string DataFileName()
        {
            if (cachedDataFileName == "")
            {
                var sceneName = Path.GetFileNameWithoutExtension(SceneManager.GetActiveScene().name);
                var objectName = gameObject.name;
                var fileName = string.Format("{0}_{1}.probes", sceneName, objectName);
                var filePath = Path.Combine(Application.streamingAssetsPath, fileName);
                cachedDataFileName = filePath;
            }

            return cachedDataFileName;

            // TODO: Android loading?
        }

        public void CacheFileName()
        {
            DataFileName();
        }

        public byte[] LoadData()
        {
            var fileName = DataFileName();
            if (!File.Exists(fileName))
                return null;

            var data = File.ReadAllBytes(fileName);
            return data;
        }

        public void SaveData(byte[] data)
        {
#if UNITY_EDITOR
            if (cachedDataFileName == "" && !Directory.Exists(Application.streamingAssetsPath))
                UnityEditor.AssetDatabase.CreateFolder("Assets", "StreamingAssets");
#endif

            var fileName = DataFileName();
            File.WriteAllBytes(fileName, data);

            dataSize = data.Length;
        }

        public List<ProbeDataLayerInfo> dataLayerInfo = new List<ProbeDataLayerInfo>();

        public void ResetLayers()
        {
            dataLayerInfo.Clear();
        }

        public void AddOrUpdateLayer(BakedDataIdentifier identifier, string name, int size)
        {
            if (FindLayer(identifier) == null)
                AddLayer(identifier, name, size);
            else
                UpdateLayerSize(identifier, size);
        }

        public void AddLayer(BakedDataIdentifier identifier, string name, int size)
        {
            var layerInfo = new ProbeDataLayerInfo();
            layerInfo.identifier = identifier;
            layerInfo.name = name;
            layerInfo.size = size;

            dataLayerInfo.Add(layerInfo);
        }

        public void RemoveLayer(BakedDataIdentifier identifier)
        {
            dataLayerInfo.Remove(FindLayer(identifier));
        }

        public void UpdateLayerSize(BakedDataIdentifier identifier, int newSize)
        {
            var layerInfo = FindLayer(identifier);
            if (layerInfo != null)
                layerInfo.size = newSize;
        }

        ProbeDataLayerInfo FindLayer(BakedDataIdentifier identifier)
        {
            foreach (var layerInfo in dataLayerInfo)
            {
                if (layerInfo.identifier.identifier == identifier.identifier &&
                    layerInfo.identifier.type == identifier.type)
                {
                    return layerInfo;
                }
            }
            return null;
        }
    }

    [Serializable]
    public class ProbeDataLayerInfo
    {
        public string               name;
        public BakedDataIdentifier  identifier;
        public int                  size;
    }
}
