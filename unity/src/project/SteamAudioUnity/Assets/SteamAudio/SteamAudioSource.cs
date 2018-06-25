//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using UnityEngine;

namespace SteamAudio
{
    //
    // SourceSimulationType
    // Various simulation options for a PhononSource.
    //
    public enum SourceSimulationType
    {
        Realtime,
        BakedStaticSource,
        BakedStaticListener
    }

    [AddComponentMenu("Steam Audio/Steam Audio Source")]
    public class SteamAudioSource : MonoBehaviour
    {
        void Awake()
        {
            var steamAudioManager = SteamAudioManager.GetSingleton();
            if (steamAudioManager == null)
            {
                Debug.LogError("Phonon Manager Settings object not found in the scene! Click Window > Phonon");
                return;
            }

            steamAudioManager.Initialize(GameEngineStateInitReason.Playing);
            managerData = steamAudioManager.ManagerData();

            audioEngine = steamAudioManager.audioEngine;
            audioEngineSource = AudioEngineSourceFactory.Create(audioEngine);
            audioEngineSource.Initialize(gameObject);

            audioEngineSource.UpdateParameters(this);
        }

        void Start()
        {
            audioEngineSource.UpdateParameters(this);
        }

        void OnDestroy()
        {
            if (managerData != null)
                managerData.Destroy();

            if (audioEngineSource != null)
                audioEngineSource.Destroy();
        }

        void Update()
        {
            var requiresScene = (occlusionMode != OcclusionMode.NoOcclusion || reflections);
            var sceneExported = (managerData.gameEngineState.Scene().GetScene() != IntPtr.Zero);
            if (requiresScene && !sceneExported)
            {
                Debug.LogError("Scene not found. Make sure to pre-export the scene.");
                return;
            }

            audioEngineSource.UpdateParameters(this);

            if (audioEngineSource.ShouldSendIdentifier(this))
            {
                audioEngineSource.SendIdentifier(this, GetIdentifierToSend());
            }
        }

        public void BeginBake()
        {
            Sphere bakeSphere;
            Vector3 sphereCenter = Common.ConvertVector(gameObject.transform.position);
            bakeSphere.centerx = sphereCenter.x;
            bakeSphere.centery = sphereCenter.y;
            bakeSphere.centerz = sphereCenter.z;
            bakeSphere.radius = bakingRadius;

            GameObject[] bakeObjects = { gameObject };

            CacheIdentifier();
            BakedDataIdentifier[] bakeIdentifiers = { bakedDataIdentifier };

            string[] bakeNames = { uniqueIdentifier };
            Sphere[] bakeSpheres = { bakeSphere };

            SteamAudioProbeBox[][] bakeProbeBoxes;
            bakeProbeBoxes = new SteamAudioProbeBox[1][];

            if (useAllProbeBoxes)
                bakeProbeBoxes[0] = FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[];
            else
                bakeProbeBoxes[0] = probeBoxes;

            baker.BeginBake(bakeObjects, bakeIdentifiers, bakeNames, bakeSpheres, bakeProbeBoxes);
        }

        public void EndBake()
        {
            baker.EndBake();
        }

        void OnDrawGizmosSelected()
        {
            var steamAudioManager = FindObjectOfType<SteamAudioManager>();
            var audioEngine = (steamAudioManager) ? steamAudioManager.audioEngine : AudioEngine.UnityNative;

            if (simulationType == SourceSimulationType.BakedStaticSource || audioEngine != AudioEngine.UnityNative)
            {
                Color oldColor = Gizmos.color;
                Matrix4x4 oldMatrix = Gizmos.matrix;

                Gizmos.color = Color.yellow;
                Gizmos.DrawWireSphere(gameObject.transform.position, bakingRadius);

                Gizmos.color = Color.magenta;
                SteamAudioProbeBox[] drawProbeBoxes = probeBoxes;
                if (useAllProbeBoxes)
                    drawProbeBoxes = FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[];

                if (drawProbeBoxes != null)
                {
                    foreach (SteamAudioProbeBox probeBox in drawProbeBoxes)
                    {
                        if (probeBox == null)
                            continue;

                        Gizmos.matrix = probeBox.transform.localToWorldMatrix;
                        Gizmos.DrawWireCube(new UnityEngine.Vector3(0, 0, 0), new UnityEngine.Vector3(1, 1, 1));
                    }
                }

                Gizmos.matrix = oldMatrix;
                Gizmos.color = oldColor;
            }

            if (dipoleWeight > 0.0f)
            {
                if (deformedSphereMesh == null)
                {
                    InitializeDeformedSphereMesh(32, 32);
                }

                DeformSphereMesh();

                var oldColor = Gizmos.color;
                Gizmos.color = Color.red;
                Gizmos.DrawWireMesh(deformedSphereMesh, transform.position, transform.rotation);
                Gizmos.color = oldColor;
            }
        }

        public void UpdateBakedDataStatistics()
        {
            SteamAudioProbeBox[] statProbeBoxes = probeBoxes;
            if (useAllProbeBoxes)
                statProbeBoxes = FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[];

            if (statProbeBoxes == null)
                return;

            int dataSize = 0;
            List<string> probeNames = new List<string>();
            List<int> probeDataSizes = new List<int>();
            foreach (SteamAudioProbeBox probeBox in statProbeBoxes)
            {
                if (probeBox == null || uniqueIdentifier.Length == 0)
                    continue;

                int probeDataSize = 0;
                probeNames.Add(probeBox.name);

                for (int i = 0; i < probeBox.dataLayerInfo.Count; ++i)
                {
                    if (bakedDataIdentifier.identifier == probeBox.dataLayerInfo[i].identifier.identifier &&
                        bakedDataIdentifier.type == probeBox.dataLayerInfo[i].identifier.type)
                    {
                        probeDataSize = probeBox.dataLayerInfo[i].size;
                        dataSize += probeDataSize;
                    }
                }

                probeDataSizes.Add(probeDataSize);
            }

            bakedDataSize = dataSize;
            bakedProbeNames = probeNames;
            bakedProbeDataSizes = probeDataSizes;
        }

        void CacheIdentifier()
        {
            bakedDataIdentifier = new BakedDataIdentifier
            {
                identifier = Common.HashForIdentifier(uniqueIdentifier),
                type = BakedDataType.StaticSource
            };
        }

        int GetIdentifierToSend()
        {
            var identifier = bakedDataIdentifier;

            if (audioEngineSource.UsesBakedStaticListener(this))
            {
                var steamAudioListener = managerData.componentCache.SteamAudioListener();
                if (steamAudioListener != null)
                {
                    var staticListenerNode = steamAudioListener.currentStaticListenerNode;
                    if (staticListenerNode != null)
                        identifier = staticListenerNode.bakedDataIdentifier;
                }
            }

            return identifier.identifier;
        }

        UnityEngine.Vector3 DeformedVertex(UnityEngine.Vector3 vertex)
        {
            var cosine = vertex.z;
            var r = Mathf.Pow(Mathf.Abs((1.0f - dipoleWeight) + dipoleWeight * cosine), dipolePower);
            var deformedVertex = vertex;
            deformedVertex.Scale(new UnityEngine.Vector3(r, r, r));
            return deformedVertex;
        }

        void InitializeDeformedSphereMesh(int nPhi, int nTheta)
        {
            var dPhi = (2.0f * Mathf.PI) / nPhi;
            var dTheta = Mathf.PI / nTheta;

            sphereVertices = new UnityEngine.Vector3[nPhi * nTheta];
            var index = 0;
            for (var i = 0; i < nPhi; ++i)
            {
                var phi = i * dPhi;
                for (var j = 0; j < nTheta; ++j)
                {
                    var theta = (j * dTheta) - (0.5f * Mathf.PI);

                    var x = Mathf.Cos(theta) * Mathf.Sin(phi);
                    var y = Mathf.Sin(theta);
                    var z = Mathf.Cos(theta) * -Mathf.Cos(phi);

                    var vertex = new UnityEngine.Vector3(x, y, z);

                    sphereVertices[index++] = vertex;
                }
            }

            deformedSphereVertices = new UnityEngine.Vector3[nPhi * nTheta];
            Array.Copy(sphereVertices, deformedSphereVertices, sphereVertices.Length);

            var indices = new int[6 * nPhi * (nTheta - 1)];
            index = 0;
            for (var i = 0; i < nPhi; ++i)
            {
                for (var j = 0; j < nTheta - 1; ++j)
                {
                    var i0 = i * nTheta + j;
                    var i1 = i * nTheta + (j + 1);
                    var i2 = ((i + 1) % nPhi) * nTheta + (j + 1);
                    var i3 = ((i + 1) % nPhi) * nTheta + j;

                    indices[index++] = i0;
                    indices[index++] = i1;
                    indices[index++] = i2;
                    indices[index++] = i0;
                    indices[index++] = i2;
                    indices[index++] = i3;
                }
            }

            deformedSphereMesh = new Mesh();
            deformedSphereMesh.vertices = deformedSphereVertices;
            deformedSphereMesh.triangles = indices;
            deformedSphereMesh.RecalculateNormals();
        }

        void DeformSphereMesh()
        {
            for (var i = 0; i < sphereVertices.Length; ++i)
            {
                deformedSphereVertices[i] = DeformedVertex(sphereVertices[i]);
            }

            deformedSphereMesh.vertices = deformedSphereVertices;
        }

        public bool                 directBinaural      = true;
        public HRTFInterpolation    interpolation       = HRTFInterpolation.Nearest;
        public bool                 physicsBasedAttenuation = false;
        public bool                 airAbsorption       = false;
        [Range(0.0f, 1.0f)]
        public float                dipoleWeight        = 0.0f;
        [Range(0.0f, 4.0f)]
        public float                dipolePower         = 0.0f;
        public OcclusionMode        occlusionMode       = OcclusionMode.NoOcclusion;
        public OcclusionMethod      occlusionMethod     = OcclusionMethod.Raycast;
        [Range(0.1f, 10.0f)]
        public float                sourceRadius        = 1.0f;
        [Range(0.0f, 1.0f)]
        public float                directMixLevel      = 1.0f;

        public bool                 reflections         = false;
        public bool                 indirectBinaural    = false;
        [Range(0.0f, 10.0f)]
        public float                indirectMixLevel    = 1.0f;
        public SourceSimulationType simulationType      = SourceSimulationType.Realtime;

        public string               uniqueIdentifier    = "";

        public bool                 avoidSilenceDuringInit  = false;

        [Range(1f, 1024f)]
        public float                bakingRadius        = 16f;
        public bool                 useAllProbeBoxes    = false;
        public SteamAudioProbeBox[] probeBoxes          = null;
        public Baker                baker               = new Baker();

        public List<string>         bakedProbeNames     = new List<string>();
        public List<int>            bakedProbeDataSizes = new List<int>();
        public int                  bakedDataSize       = 0;
        public bool                 bakedStatsFoldout   = false;
        public bool                 bakeToggle          = false;

        public BakedDataIdentifier  bakedDataIdentifier;

        ManagerData                 managerData         = null;
        AudioEngine                 audioEngine         = AudioEngine.UnityNative;

        AudioEngineSource           audioEngineSource   = null;

        UnityEngine.Vector3[]       sphereVertices          = null;
        UnityEngine.Vector3[]       deformedSphereVertices  = null;
        Mesh                        deformedSphereMesh      = null;
    }
}