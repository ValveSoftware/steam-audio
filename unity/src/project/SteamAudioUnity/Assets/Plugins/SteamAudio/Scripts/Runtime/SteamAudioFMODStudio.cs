//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace SteamAudio
{
    public static class FMODStudioAPI
    {
        // FMOD STUDIO PLUGIN

#if UNITY_IOS && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon_fmod")]
#endif
        public static extern void iplFMODInitialize(IntPtr context);

#if UNITY_IOS && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon_fmod")]
#endif
        public static extern void iplFMODSetHRTF(IntPtr hrtf);

#if UNITY_IOS && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon_fmod")]
#endif
        public static extern void iplFMODSetSimulationSettings(SimulationSettings simulationSettings);

#if UNITY_IOS && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon_fmod")]
#endif
        public static extern void iplFMODSetReverbSource(IntPtr reverbSource);

#if UNITY_IOS && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon_fmod")]
#endif
        public static extern void iplFMODTerminate();

#if UNITY_IOS && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon_fmod")]
#endif
        public static extern int iplFMODAddSource(IntPtr source);

#if UNITY_IOS && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon_fmod")]
#endif
        public static extern void iplFMODRemoveSource(int handle);
    }
}
