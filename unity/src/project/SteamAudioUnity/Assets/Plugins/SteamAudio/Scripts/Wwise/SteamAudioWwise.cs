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

namespace SteamAudio
{
    public struct WwiseSettings
    {
        public float metersPerUnit;
    }

    public static class WwiseAPI
    {
        // WWISE PLUGIN

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("SteamAudioWwise")]
#endif
        public static extern void iplWwiseInitialize(IntPtr context, ref WwiseSettings wwiseSettings);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("SteamAudioWwise")]
#endif
        public static extern void iplWwiseSetHRTF(IntPtr hrtf);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("SteamAudioWwise")]
#endif
        public static extern void iplWwiseSetSimulationSettings(SimulationSettings simulationSettings);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("SteamAudioWwise")]
#endif
        public static extern void iplWwiseSetReverbSource(IntPtr reverbSource);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("SteamAudioWwise")]
#endif
        public static extern void iplWwiseTerminate();

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("SteamAudioWwise")]
#endif
        public static extern void iplWwiseAddSource(ulong gameObjectID, IntPtr source);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("SteamAudioWwise")]
#endif
        public static extern void iplWwiseRemoveSource(ulong gameObjectID);
    }
}
