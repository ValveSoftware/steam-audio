//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using System.Collections.Generic;

namespace SteamAudio
{
    //
    // Phonon Static Listener
    // Represents a baked static listener component.
    //

    [AddComponentMenu("Steam Audio/Steam Audio Baked Static Listener Node")]
    public class SteamAudioBakedStaticListenerNode : MonoBehaviour
    {
        void OnDrawGizmosSelected()
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
                type = BakedDataType.StaticListener
            };
        }

        // Public members.
        public string uniqueIdentifier = "";
        [Range(1f, 1024f)]
        public float bakingRadius = 16f;
        public bool useAllProbeBoxes = false;
        public SteamAudioProbeBox[] probeBoxes = null;
        public Baker phononBaker = new Baker();

        // Public stored fields - baking.
        public List<string> bakedProbeNames = new List<string>();
        public List<int> bakedProbeDataSizes = new List<int>();
        public int bakedDataSize = 0;
        public bool bakeToggle = false;
        public bool bakedStatsFoldout = false;

        public BakedDataIdentifier bakedDataIdentifier;
    }
}
