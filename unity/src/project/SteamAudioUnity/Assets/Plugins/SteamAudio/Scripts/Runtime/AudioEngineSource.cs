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
#if STEAMAUDIO_ENABLED

using System;
using UnityEngine;

namespace SteamAudio
{
    public abstract class AudioEngineSource
    {
        public virtual void Initialize(GameObject gameObject)
        { }

        public virtual void Destroy()
        { }

        public virtual void UpdateParameters(SteamAudioSource source)
        { }

        public virtual void GetParameters(SteamAudioSource source)
        { }

        public static AudioEngineSource Create(AudioEngineType type)
        {
            switch (type)
            {
            case AudioEngineType.Unity:
                return new UnityAudioEngineSource();
            case AudioEngineType.FMODStudio:
                return CreateFMODStudioAudioEngineSource();
            case AudioEngineType.Wwise:
                return CreateWwiseAudioEngineSource();
            default:
                return null;
            }
        }

        private static AudioEngineSource CreateFMODStudioAudioEngineSource()
        {
            var type = Type.GetType("SteamAudio.FMODStudioAudioEngineSource,SteamAudioFMODStudio");
            return (type != null) ? (AudioEngineSource) Activator.CreateInstance(type) : null;
        }

        private static AudioEngineSource CreateWwiseAudioEngineSource()
        {
            var type = Type.GetType("SteamAudio.WwiseAudioEngineSource,SteamAudioWwiseUnity");
            if (type == null)
                return null;

            return (AudioEngineSource) Activator.CreateInstance(type);
        }
    }
}

#endif
