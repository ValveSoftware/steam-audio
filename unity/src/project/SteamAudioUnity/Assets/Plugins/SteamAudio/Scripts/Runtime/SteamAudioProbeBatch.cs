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

#if STEAMAUDIO_ENABLED
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

        private void OnDestroy()
        {
            if (mProbeBatch != null)
            {
                mProbeBatch.Release();
            }
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
                staticMeshComponent = rootObject.GetComponentInChildren<SteamAudioStaticMesh>();
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
            probeGenerationParams.transform = Common.ConvertTransform(gameObject.transform);

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

            probeBatch.Release();
            probeArray.Release();
            staticMesh.Release();

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
            probeBatch.Release();

            SteamAudioManager.ShutDown();
            DestroyImmediate(SteamAudioManager.Singleton.gameObject);

            RemoveLayer(identifier);
        }

        public int GetSizeForLayer(BakedDataIdentifier identifier)
        {
            for (int i = 0; i < mBakedDataLayerInfo.Count; ++i)
            {
                if (mBakedDataLayerInfo[i].identifier.Equals(identifier))
                {
                    return mBakedDataLayerInfo[i].dataSize;
                }

            }

            return 0;
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

        public void RemoveLayer(BakedDataIdentifier identifier)
        {
            for (int i = 0; i < mBakedDataLayerInfo.Count; ++i)
            {
                if (mBakedDataLayerInfo[i].identifier.Equals(identifier))
                {
                    var layerInfo = mBakedDataLayerInfo[i];
                    mBakedDataLayerInfo.RemoveAt(i);
                    UpdateGameObjectStatistics(layerInfo);
                    return;
                }
            }
        }

        public void AddOrUpdateLayer(GameObject gameObject, BakedDataIdentifier identifier, int dataSize)
        {
            for (int i = 0; i < mBakedDataLayerInfo.Count; ++i)
            {
                if (mBakedDataLayerInfo[i].identifier.Equals(identifier))
                {
                    var layerInfo = mBakedDataLayerInfo[i];
                    layerInfo.dataSize = dataSize;
                    mBakedDataLayerInfo[i] = layerInfo;
                    return;
                }
            }

            AddLayer(gameObject, identifier, dataSize);
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
#endif
    }
}
