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
using System.Reflection;
using UnityEngine;

namespace SteamAudio
{
    public sealed class WwiseAudioEngineSource : AudioEngineSource
    {
        SteamAudioSource mSteamAudioSource = null;
        ulong mWwiseGameObjectID = 0;

        public override void Initialize(GameObject gameObject)
        {
            mSteamAudioSource = gameObject.GetComponent<SteamAudioSource>();
            if (mSteamAudioSource == null)
                return;

            var argsGetAkGameObjectID = new object[] { gameObject };
            mWwiseGameObjectID = AkSoundEngine.GetAkGameObjectID(gameObject);
            if (mWwiseGameObjectID == AkSoundEngine.AK_INVALID_GAME_OBJECT)
                return;

            WwiseAPI.iplWwiseAddSource(mWwiseGameObjectID, mSteamAudioSource.GetSource().Get());
        }

        public override void Destroy()
        {
            if (mSteamAudioSource == null || mWwiseGameObjectID == AkSoundEngine.AK_INVALID_GAME_OBJECT)
                return;

            WwiseAPI.iplWwiseRemoveSource(mWwiseGameObjectID);
        }

        public override void UpdateParameters(SteamAudioSource source)
        {
            // Nothing to do here, simulation outputs are linked via AkGameObjectID.
        }
    }
}

#endif
