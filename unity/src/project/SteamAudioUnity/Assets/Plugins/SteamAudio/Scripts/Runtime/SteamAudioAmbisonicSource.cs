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
using UnityEngine;

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Ambisonic Source")]
    [RequireComponent(typeof(AudioSource))]
    public class SteamAudioAmbisonicSource : MonoBehaviour
    {
        [Header("HRTF Settings")]
        public bool applyHRTF = true;

        AudioEngineAmbisonicSource mAudioEngineAmbisonicSource = null;

        private void Awake()
        {
            mAudioEngineAmbisonicSource = AudioEngineAmbisonicSource.Create(SteamAudioSettings.Singleton.audioEngine);
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.Initialize(gameObject);
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

        private void Start()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

        private void OnDestroy()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.Destroy();
            }
        }

        private void OnEnable()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

        private void Update()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

    }
}
