//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Collections;
using System.Collections.Generic;
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
            audioSource = GetComponent<AudioSource>();

            var steamAudioManager = FindObjectOfType<SteamAudioManager>();
            if (steamAudioManager == null)
            {
                Debug.LogError("Phonon Manager Settings object not found in the scene! Click Window > Phonon");
                return;
            }

            steamAudioManager.Initialize(GameEngineStateInitReason.Playing);

            audioEngine = steamAudioManager.audioEngine;
            managerData = steamAudioManager.ManagerData();
        }

        void OnDestroy()
        {
            if (managerData != null)
                managerData.Destroy();
        }

        void Update()
        {
            if (!audioSource || !audioSource.isPlaying)
                return;

            if (audioEngine != AudioEngine.UnityNative)
                return;

            var requiresScene = (occlusionMode != OcclusionMode.NoOcclusion || reflections);
            var sceneExported = (managerData.gameEngineState.Scene().GetScene() != IntPtr.Zero);
            if (requiresScene && !sceneExported)
            {
                Debug.LogError("Scene not found. Make sure to pre-export the scene.");
                return;
            }

            var identifier = uniqueIdentifier;
            if (simulationType == SourceSimulationType.BakedStaticListener)
            {
                var steamAudioListener = managerData.componentCache.SteamAudioListener();
                if (steamAudioListener != null)
                {
                    var staticListenerNode = steamAudioListener.currentStaticListenerNode;
                    if (staticListenerNode != null)
                        identifier = staticListenerNode.uniqueIdentifier;
                }
            }

            var identifierInt = Common.HashForIdentifier(identifier);
            var identifierFloat = BitConverter.ToSingle(BitConverter.GetBytes(identifierInt), 0);

            var directBinauralValue = (directBinaural) ? 1.0f : 0.0f;
            var hrtfInterpolationValue = (float)interpolation;
            var distanceAttenuationValue = (physicsBasedAttenuation) ? 1.0f : 0.0f;
            var airAbsorptionValue = (airAbsorption) ? 1.0f : 0.0f;
            var occlusionModeValue = (float)occlusionMode;
            var occlusionMethodValue = (float)occlusionMethod;
            var sourceRadiusValue = sourceRadius;
            var directMixLevelValue = directMixLevel;
            var reflectionsValue = (reflections) ? 1.0f : 0.0f;
            var indirectBinauralValue = (indirectBinaural) ? 1.0f : 0.0f;
            var indirectMixLevelValue = indirectMixLevel;
            var simulationTypeValue = (simulationType == SourceSimulationType.Realtime) ? 0.0f : 1.0f;
            var nameValue = identifierFloat;
            var usesStaticListenerValue = (simulationType == SourceSimulationType.BakedStaticListener) ? 1.0f : 0.0f;

            audioSource.SetSpatializerFloat(0, directBinauralValue);
            audioSource.SetSpatializerFloat(1, hrtfInterpolationValue);
            audioSource.SetSpatializerFloat(2, distanceAttenuationValue);
            audioSource.SetSpatializerFloat(3, airAbsorptionValue);
            audioSource.SetSpatializerFloat(4, occlusionModeValue);
            audioSource.SetSpatializerFloat(5, occlusionMethodValue);
            audioSource.SetSpatializerFloat(6, sourceRadiusValue);
            audioSource.SetSpatializerFloat(7, directMixLevelValue);
            audioSource.SetSpatializerFloat(8, reflectionsValue);
            audioSource.SetSpatializerFloat(9, indirectBinauralValue);
            audioSource.SetSpatializerFloat(10, indirectMixLevelValue);
            audioSource.SetSpatializerFloat(11, simulationTypeValue);
            audioSource.SetSpatializerFloat(12, usesStaticListenerValue);
            audioSource.SetSpatializerFloat(13, nameValue);
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
            BakingMode[] bakingModes = { BakingMode.StaticSource };
            string[] bakeStrings = { uniqueIdentifier };
            Sphere[] bakeSpheres = { bakeSphere };

            SteamAudioProbeBox[][] bakeProbeBoxes;
            bakeProbeBoxes = new SteamAudioProbeBox[1][];

            if (useAllProbeBoxes)
                bakeProbeBoxes[0] = FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[];
            else
                bakeProbeBoxes[0] = probeBoxes;

            baker.BeginBake(bakeObjects, bakingModes, bakeStrings, bakeSpheres, bakeProbeBoxes);
        }

        public void EndBake()
        {
            baker.EndBake();
        }

        void OnDrawGizmosSelected()
        {
            if (simulationType == SourceSimulationType.BakedStaticSource)
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

                for (int i = 0; i < probeBox.probeDataName.Count; ++i)
                {
                    if (uniqueIdentifier == probeBox.probeDataName[i])
                    {
                        probeDataSize = probeBox.probeDataNameSizes[i];
                        dataSize += probeDataSize;
                    }
                }

                probeDataSizes.Add(probeDataSize);
            }

            bakedDataSize = dataSize;
            bakedProbeNames = probeNames;
            bakedProbeDataSizes = probeDataSizes;
        }

        public bool                 directBinaural      = true;
        public HRTFInterpolation    interpolation       = HRTFInterpolation.Nearest;
        public bool                 physicsBasedAttenuation = false;
        public bool                 airAbsorption       = false;
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

        AudioSource                 audioSource         = null;
        ManagerData                 managerData         = null;
        AudioEngine                 audioEngine         = AudioEngine.UnityNative;
    }
}