//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace SteamAudio
{
    [Serializable]
    public struct BakedDataLayerInfo
    {
        public GameObject gameObject;
        public BakedDataIdentifier identifier;
        public int dataSize;
    }

    [AddComponentMenu("Steam Audio/Steam Audio Probe Batch")]
    public class SteamAudioProbeBatch : MonoBehaviour
    {
        [Header("Placement Settings")]
        public ProbeGenerationType placementStrategy = ProbeGenerationType.UniformFloor;
        [Range(.1f, 50f)]
        public float horizontalSpacing = 5f;
        [Range(.1f, 20f)]
        public float heightAboveFloor = 1.5f;

        [Header("Export Settings")]
        public SerializedData asset = null;

        public int probeDataSize = 0;
        [SerializeField] Sphere[] mProbeSpheres = null;
        [SerializeField] List<BakedDataLayerInfo> mBakedDataLayerInfo = new List<BakedDataLayerInfo>();

        ProbeBatch mProbeBatch = null;

        const float kProbeDrawSize = 0.1f;

        public SerializedData GetAsset()
        {
            if (asset == null)
            {
                asset = SerializedData.PromptForNewAsset(gameObject.scene.name + "_" + name);
            }

            return asset;
        }

        public int GetNumProbes()
        { 
            return (mProbeSpheres == null) ? 0 : mProbeSpheres.Length; 
        }

        public int GetNumLayers()
        {
            return mBakedDataLayerInfo.Count;
        }

        public IntPtr GetProbeBatch()
        {
            return mProbeBatch.Get();
        }

        private void Awake()
        {
            if (asset == null)
                return;

            mProbeBatch = new ProbeBatch(SteamAudioManager.Context, asset);
            mProbeBatch.Commit();
        }

        private void OnEnable()
        {
            SteamAudioManager.Simulator.AddProbeBatch(mProbeBatch);
        }

        private void OnDisable()
        {
            if (SteamAudioManager.Simulator != null)
            {
                SteamAudioManager.Simulator.RemoveProbeBatch(mProbeBatch);
            }
        }

        void OnDrawGizmosSelected()
        {
            var oldColor = Gizmos.color;
            Gizmos.color = Color.magenta;

            var oldMatrix = Gizmos.matrix;
            Gizmos.matrix = transform.localToWorldMatrix;
            Gizmos.DrawWireCube(new UnityEngine.Vector3(0, 0, 0), new UnityEngine.Vector3(1, 1, 1));
            Gizmos.matrix = oldMatrix;

            Gizmos.color = Color.yellow;
            if (mProbeSpheres != null)
            {
                for (var i = 0; i < mProbeSpheres.Length; ++i)
                {
                    var center = Common.ConvertVector(mProbeSpheres[i].center);
                    Gizmos.DrawCube(center, new UnityEngine.Vector3(kProbeDrawSize, kProbeDrawSize, kProbeDrawSize));
                }
            }
            Gizmos.color = oldColor;
        }

        public void GenerateProbes()
        {
            SteamAudioManager.Initialize(ManagerInitReason.GeneratingProbes);
            SteamAudioManager.LoadScene(SceneManager.GetActiveScene(), SteamAudioManager.Context, false);
            var scene = SteamAudioManager.CurrentScene;

            SteamAudioStaticMesh staticMeshComponent = null;
            var rootObjects = SceneManager.GetActiveScene().GetRootGameObjects();
            foreach (var rootObject in rootObjects)
            {
                staticMeshComponent = rootObject.GetComponent<SteamAudioStaticMesh>();
                if (staticMeshComponent)
                    break;
            }

            if (staticMeshComponent == null || staticMeshComponent.asset == null)
            {
                Debug.LogError(string.Format("Scene {0} has not been exported. Click Steam Audio > Export Active Scene to do so.", SceneManager.GetActiveScene().name));
                return;
            }

            var staticMesh = new StaticMesh(SteamAudioManager.Context, scene, staticMeshComponent.asset);
            staticMesh.AddToScene(scene);

            scene.Commit();

            var probeArray = new ProbeArray(SteamAudioManager.Context);

            var probeGenerationParams = new ProbeGenerationParams { };
            probeGenerationParams.type = placementStrategy;
            probeGenerationParams.spacing = horizontalSpacing;
            probeGenerationParams.height = heightAboveFloor;
            probeGenerationParams.transform = Common.TransposeMatrix(Common.ConvertTransform(gameObject.transform)); // Probe generation requires a transposed matrix.

            probeArray.GenerateProbes(scene, probeGenerationParams);

            var numProbes = probeArray.GetNumProbes();
            mProbeSpheres = new Sphere[numProbes];
            for (var i = 0; i < numProbes; ++i)
            {
                mProbeSpheres[i] = probeArray.GetProbe(i);
            }

            var probeBatch = new ProbeBatch(SteamAudioManager.Context);
            probeBatch.AddProbeArray(probeArray);

            probeDataSize = probeBatch.Save(GetAsset());

            SteamAudioManager.ShutDown();
            DestroyImmediate(SteamAudioManager.Singleton.gameObject);

            ResetLayers();

            Debug.Log("Generated " + mProbeSpheres.Length + " probes for game object " + gameObject.name + ".");

            // Redraw scene view for probes to show up instantly.
#if UNITY_EDITOR
            SceneView.RepaintAll();
#endif
        }

        public void DeleteBakedDataForIdentifier(BakedDataIdentifier identifier)
        {
            if (asset == null)
                return;

            SteamAudioManager.Initialize(ManagerInitReason.EditingProbes);

            var probeBatch = new ProbeBatch(SteamAudioManager.Context, asset);
            probeBatch.RemoveData(identifier);
            probeDataSize = probeBatch.Save(asset);

            SteamAudioManager.ShutDown();
            DestroyImmediate(SteamAudioManager.Singleton.gameObject);

            RemoveLayer(identifier);
        }

        public int GetSizeForLayer(BakedDataIdentifier identifier)
        {
            var layerInfo = new BakedDataLayerInfo { };
            if (FindLayer(identifier, ref layerInfo))
            {
                return layerInfo.dataSize;
            }
            else
            {
                return 0;
            }
        }

        public BakedDataLayerInfo GetInfoForLayer(int index)
        {
            return mBakedDataLayerInfo[index];
        }

        public void ResetLayers()
        {
            mBakedDataLayerInfo.Clear();
        }

        public void AddLayer(GameObject gameObject, BakedDataIdentifier identifier, int dataSize)
        {
            var layerInfo = new BakedDataLayerInfo { };
            layerInfo.gameObject = gameObject;
            layerInfo.identifier = identifier;
            layerInfo.dataSize = dataSize;

            mBakedDataLayerInfo.Add(layerInfo);
        }

        public void UpdateLayer(BakedDataIdentifier identifier, int dataSize)
        {
            var layerInfo = new BakedDataLayerInfo { };
            if (FindLayer(identifier, ref layerInfo))
            {
                layerInfo.dataSize = dataSize;
            }
        }

        public void RemoveLayer(BakedDataIdentifier identifier)
        {
            var layerInfo = new BakedDataLayerInfo { };
            if (FindLayer(identifier, ref layerInfo))
            {
                mBakedDataLayerInfo.Remove(layerInfo);
                UpdateGameObjectStatistics(layerInfo);
            }
        }

        public void AddOrUpdateLayer(GameObject gameObject, BakedDataIdentifier identifier, int dataSize)
        {
            var layerInfo = new BakedDataLayerInfo { };
            if (FindLayer(identifier, ref layerInfo))
            {
                UpdateLayer(identifier, dataSize);
            }
            else
            {
                AddLayer(gameObject, identifier, dataSize);
            }
        }

        bool FindLayer(BakedDataIdentifier identifier, ref BakedDataLayerInfo result)
        {
            foreach (var layerInfo in mBakedDataLayerInfo)
            {
                if (layerInfo.identifier.Equals(identifier))
                {
                    result = layerInfo;
                    return true;
                }
            }

            return false;
        }

        void UpdateGameObjectStatistics(BakedDataLayerInfo layerInfo)
        {
            if (layerInfo.identifier.type == BakedDataType.Reflections)
            {
                switch (layerInfo.identifier.variation)
                {
                case BakedDataVariation.Reverb:
                    layerInfo.gameObject.GetComponent<SteamAudioListener>().UpdateBakedDataStatistics();
                    break;

                case BakedDataVariation.StaticSource:
                    layerInfo.gameObject.GetComponent<SteamAudioBakedSource>().UpdateBakedDataStatistics();
                    break;

                case BakedDataVariation.StaticListener:
                    layerInfo.gameObject.GetComponent<SteamAudioBakedListener>().UpdateBakedDataStatistics();
                    break;
                }
            }
        }

        BakedDataIdentifier GetBakedDataIdentifier()
        {
            var identifier = new BakedDataIdentifier { };
            identifier.type = BakedDataType.Pathing;
            identifier.variation = BakedDataVariation.Dynamic;
            return identifier;
        }

        public void BeginBake()
        {
            var tasks = new BakedDataTask[1];
            tasks[0].gameObject = gameObject;
            tasks[0].component = this;
            tasks[0].name = gameObject.name;
            tasks[0].identifier = GetBakedDataIdentifier();
            tasks[0].probeBatches = new SteamAudioProbeBatch[1];
            tasks[0].probeBatchNames = new string[1];
            tasks[0].probeBatchAssets = new SerializedData[1];

            tasks[0].probeBatches[0] = this;
            tasks[0].probeBatchNames[0] = gameObject.name;
            tasks[0].probeBatchAssets[0] = GetAsset();

            Baker.BeginBake(tasks);
        }
    }
}
