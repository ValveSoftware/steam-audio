//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using System;

namespace SteamAudio
{
    public class ProbeManager
    {
        public IntPtr GetProbeManager()
        {
            return probeManager;
        }

        public Error Create(IntPtr context)
        {
            var error = PhononCore.iplCreateProbeBatch(context, ref probeBatch);
            if (error != Error.None)
            {
                throw new Exception("Unable to create probe batch.");
            }

            error = PhononCore.iplCreateProbeManager(context, ref probeManager);
            if (error != Error.None)
            {
                throw new Exception("Unable to create probe batch.");
            }

            //Add all probes from all probe boxes to the probe batch.
            SteamAudioProbeBox[] allProbeBoxes = GameObject.FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[];
            foreach (SteamAudioProbeBox probeBox in allProbeBoxes)
            {
                var probeBoxData = probeBox.LoadData();

                if (probeBoxData == null || probeBoxData.Length == 0)
                    continue;

                IntPtr probeBoxPtr = IntPtr.Zero;
                try
                {
                    PhononCore.iplLoadProbeBox(context, probeBoxData, probeBoxData.Length, ref probeBoxPtr);
                }
                catch (Exception e)
                {
                    Debug.LogError(e.Message);
                }

                int numProbes = PhononCore.iplGetProbeSpheres(probeBoxPtr, null);
                for (int i=0; i<numProbes; ++i)
                    PhononCore.iplAddProbeToBatch(probeBatch, probeBoxPtr, i);

                PhononCore.iplDestroyProbeBox(ref probeBoxPtr);
            }

            //Add probe batch to probe manager.
            PhononCore.iplAddProbeBatch(probeManager, probeBatch);
            PhononCore.iplFinalizeProbeBatch(probeBatch);

            return error;
        }

        public void Destroy()
        {
            if (probeBatch != IntPtr.Zero)
            {
                PhononCore.iplDestroyProbeBatch(ref probeBatch);
                probeBatch = IntPtr.Zero;
            }

            if (probeManager != IntPtr.Zero)
            {
                PhononCore.iplDestroyProbeManager(ref probeManager);
                probeManager = IntPtr.Zero;
            }
        }

        IntPtr probeManager = IntPtr.Zero;
        IntPtr probeBatch = IntPtr.Zero;
    }
}