//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using System;
using System.Collections.Generic;

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
            PhononCore.iplCreateProbeBox(steamAudioManager.GameEngineState().Scene().GetScene(),
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
            probeBoxData = new byte[probeBoxSize];
            PhononCore.iplSaveProbeBox(probeBox, probeBoxData);

            if (steamAudioManager.GameEngineState().Scene().GetScene() != IntPtr.Zero)
                Debug.Log("Generated " + probeSpheres.Length + " probes for game object " + gameObject.name + ".");

            // Cleanup.
            PhononCore.iplDestroyProbeBox(ref probeBox);
            steamAudioManager.Destroy();
            ClearProbeDataMapping();

            // Redraw scene view for probes to show up instantly.
#if UNITY_EDITOR
            UnityEditor.SceneView.RepaintAll();
#endif
        }

        public void DeleteBakedDataByName(string name, string prefix)
        {
            IntPtr probeBox = IntPtr.Zero;
            try
            {
                PhononCore.iplLoadProbeBox(probeBoxData, probeBoxData.Length, ref probeBox);
                PhononCore.iplDeleteBakedDataByName(probeBox, Common.HashStringForIdentifierWithPrefix(name, prefix));
                UpdateProbeDataMapping(name, prefix, -1);

                int probeBoxSize = PhononCore.iplSaveProbeBox(probeBox, null);
                probeBoxData = new byte[probeBoxSize];
                PhononCore.iplSaveProbeBox(probeBox, probeBoxData);
            }
            catch (Exception e)
            {
                Debug.LogError(e.Message);
            }

        }

        public void UpdateProbeDataMapping(string name, string prefix, int size)
        {
            int index = probeDataName.IndexOf(name);

            if (size == -1 && index >= 0)
            {
                probeDataName.RemoveAt(index);
                probeDataNamePrefixes.RemoveAt(index);
                probeDataNameSizes.RemoveAt(index);
            }
            else if (index == -1)
            {
                probeDataName.Add(name);
                probeDataNamePrefixes.Add(prefix);
                probeDataNameSizes.Add(size);
            }
            else
            {
                probeDataNameSizes[index] = size;
            }
        }

        void ClearProbeDataMapping()
        {
            probeDataName.Clear();
            probeDataNameSizes.Clear();
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

        public byte[] probeBoxData = null;
        public float[] probeSpherePoints = null;
        public float[] probeSphereRadii = null;

        public List<string> probeDataName = new List<string>();
        public List<string> probeDataNamePrefixes = new List<string>();
        public List<int> probeDataNameSizes = new List<int>();
    }
}
