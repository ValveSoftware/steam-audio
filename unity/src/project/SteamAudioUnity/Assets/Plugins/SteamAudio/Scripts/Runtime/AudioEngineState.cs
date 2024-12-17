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
    public abstract class AudioEngineState
    {
        public virtual void Initialize(IntPtr context, IntPtr defaultHRTF, SimulationSettings simulationSettings, PerspectiveCorrection correction)
        { }

        public virtual void Destroy()
        { }

        public virtual void SetHRTF(IntPtr hrtf)
        { }

        public virtual void SetPerspectiveCorrection(PerspectiveCorrection correction)
        { }

        public virtual void SetReverbSource(Source reverbSource)
        { }

        public static AudioEngineState Create(AudioEngineType type)
        {
            switch (type)
            {
            case AudioEngineType.Unity:
                return new UnityAudioEngineState();
            case AudioEngineType.FMODStudio:
                return CreateFMODStudioAudioEngineState();
            case AudioEngineType.Wwise:
                return CreateWwiseAudioEngineState();
            default:
                return null;
            }
        }

        private static AudioEngineState CreateFMODStudioAudioEngineState()
        {
            var type = Type.GetType("SteamAudio.FMODStudioAudioEngineState,SteamAudioFMODStudio");
            return (type != null) ? (AudioEngineState) Activator.CreateInstance(type) : null;
        }

        private static AudioEngineState CreateWwiseAudioEngineState()
        {
            var type = Type.GetType("SteamAudio.WwiseAudioEngineState,SteamAudioWwiseUnity");
            if (type == null)
                return null;

            return (AudioEngineState) Activator.CreateInstance(type);
        }

        public virtual void SetHRTFDisabled(bool disabled) 
        { }
    }

    public abstract class AudioEngineStateHelpers
    {
        public abstract Transform GetListenerTransform();

        public abstract SteamAudio.AudioSettings GetAudioSettings();

        public static AudioEngineStateHelpers Create(AudioEngineType type)
        {
            switch (type)
            {
                case AudioEngineType.Unity:
                    return new UnityAudioEngineStateHelpers();
                case AudioEngineType.FMODStudio:
                    return CreateFMODStudioAudioEngineStateHelpers();
                case AudioEngineType.Wwise:
                    return CreateWwiseAudioEngineStateHelpers();
                default:
                    return null;
            }
        }

        private static AudioEngineStateHelpers CreateFMODStudioAudioEngineStateHelpers()
        {
            var type = Type.GetType("SteamAudio.FMODStudioAudioEngineStateHelpers,SteamAudioFMODStudio");
            return (type != null) ? (AudioEngineStateHelpers) Activator.CreateInstance(type) : null;
        }

        private static AudioEngineStateHelpers CreateWwiseAudioEngineStateHelpers()
        {
            var type = Type.GetType("SteamAudio.WwiseAudioEngineStateHelpers,SteamAudioWwiseUnity");
            if (type == null)
                return null;

            return (AudioEngineStateHelpers) Activator.CreateInstance(type);
        }
    }
}

#endif
