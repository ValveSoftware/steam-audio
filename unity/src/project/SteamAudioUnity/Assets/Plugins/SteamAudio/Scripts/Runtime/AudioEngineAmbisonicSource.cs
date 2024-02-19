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

using UnityEngine;

namespace SteamAudio
{
    public abstract class AudioEngineAmbisonicSource
    {
        public virtual void Initialize(GameObject gameObject)
        { }

        public virtual void Destroy()
        { }

        public virtual void UpdateParameters(SteamAudioAmbisonicSource ambisonicSource)
        { }

        public virtual void GetParameters(SteamAudioAmbisonicSource ambisonicSource)
        { }

        public static AudioEngineAmbisonicSource Create(AudioEngineType type)
        {
            switch (type)
            {
            case AudioEngineType.Unity:
                return new UnityAudioEngineAmbisonicSource();
            default:
                return null;
            }
        }
    }
}
