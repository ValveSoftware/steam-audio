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
