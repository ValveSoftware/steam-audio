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
    public enum ReverbSimulationType
    {
        RealtimeReverb,
        BakedReverb,
    }

    //
    // PhononListener
    // Represents a Phonon Listener. Performs optimized mixing in fourier
    // domain or apply reverb.
    //

    [AddComponentMenu("Steam Audio/Steam Audio Listener")]
    public class SteamAudioListener : MonoBehaviour
    {
        void OnDrawGizmosSelected()
        {
            Color oldColor = Gizmos.color;
            Matrix4x4 oldMatrix = Gizmos.matrix;

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

        public void BeginBake()
        {
            GameObject[] bakeObjects = { gameObject };

            BakedDataIdentifier[] bakeIdentifiers = { Identifier };
            string[] bakeNames = { "reverb" };
            Sphere[] bakeSpheres = { new Sphere() };

            SteamAudioProbeBox[][] bakeProbeBoxes;
            bakeProbeBoxes = new SteamAudioProbeBox[1][];

            if (useAllProbeBoxes)
                bakeProbeBoxes[0] = FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[];
            else
                bakeProbeBoxes[0] = probeBoxes;

            phononBaker.BeginBake(bakeObjects, bakeIdentifiers, bakeNames, bakeSpheres, bakeProbeBoxes);
        }

        public void EndBake()
        {
            phononBaker.EndBake();
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
                if (probeBox == null)
                    continue;

                int probeDataSize = 0;
                probeNames.Add(probeBox.name);

                for (int i = 0; i < probeBox.dataLayerInfo.Count; ++i)
                {
                    if (Identifier.type == probeBox.dataLayerInfo[i].identifier.type)
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

        public BakedDataIdentifier Identifier
        {
            get
            {
                return new BakedDataIdentifier
                {
                    identifier = 0,
                    type = BakedDataType.Reverb
                };
            }
        }

        // Public members.
        public bool useAllProbeBoxes = false;
        public SteamAudioProbeBox[] probeBoxes = null;

        // Public stored fields - baking.
        public List<string> bakedProbeNames = new List<string>();
        public List<int> bakedProbeDataSizes = new List<int>();
        public int bakedDataSize = 0;
        public bool bakedStatsFoldout = false;
        public bool bakeToggle = false;

        public SteamAudioBakedStaticListenerNode currentStaticListenerNode;
 
        public Baker phononBaker = new Baker();
    }
}