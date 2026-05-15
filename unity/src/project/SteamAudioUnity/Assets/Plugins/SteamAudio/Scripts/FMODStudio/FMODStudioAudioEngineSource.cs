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
    public sealed class FMODStudioAudioEngineSource : AudioEngineSource
    {
        bool mFoundDSP = false;
        FMODUnity.StudioEventEmitter mEventEmitter = null;
        FMOD.Studio.EventInstance mEventInstance;
        FMOD.DSP mDSP;
        SteamAudioSource mSteamAudioSource = null;
        int mHandle = -1;

        const int kSimulationOutputsParamIndex = 33;

        public override void Initialize(GameObject gameObject)
        {
            mFoundDSP = false;
            mDSP = default;
            
            FindDSP(gameObject);

            mSteamAudioSource = gameObject.GetComponent<SteamAudioSource>();
            if (mSteamAudioSource)
            {
                mHandle = FMODStudioAPI.iplFMODAddSource(mSteamAudioSource.GetSource().Get());
            }
        }

        public override void Destroy()
        {
            mFoundDSP = false;
            mDSP = default;

            if (mSteamAudioSource)
            {
                FMODStudioAPI.iplFMODRemoveSource(mHandle);
            }
        }

        public override void UpdateParameters(SteamAudioSource source)
        {
            CheckForChangedEventInstance();

            if (!mFoundDSP || !mDSP.hasHandle())
            {
                mFoundDSP = false;
                mDSP = default;
                FindDSP(source.gameObject);
            }

            if (!mFoundDSP || !mDSP.hasHandle())
                return;

            var result = mDSP.setParameterInt(kSimulationOutputsParamIndex, mHandle);

            if (result != FMOD.RESULT.OK)
            {
                mFoundDSP = false;
                mDSP = default;
            }
        }

        void CheckForChangedEventInstance()
        {
            if (mEventEmitter != null)
            {
                var eventInstance = mEventEmitter.EventInstance;
                if (!eventInstance.isValid() || eventInstance.handle != mEventInstance.handle)
                {
                    // The event instance is different from the one we last used, which most likely means the
                    // event-related objects were destroyed and re-created. Make sure we look for the DSP instance
                    // when FindDSP is called next.
                    mFoundDSP = false;
                    mDSP = default;
                    mEventInstance = default;
                }
            }
            else
            {
                // We haven't yet seen a valid event emitter component, so make sure we look for one when
                // FindDSP is called.
                mFoundDSP = false;
                mDSP = default;
                mEventInstance = default;
            }
        }

        void FindDSP(GameObject gameObject)
        {
            if (mFoundDSP)
                return;

            mEventEmitter = gameObject.GetComponent<FMODUnity.StudioEventEmitter>();
            if (mEventEmitter == null)
                return;

            mEventInstance = mEventEmitter.EventInstance;
            if (!mEventInstance.isValid())
                return;

            FMOD.ChannelGroup channelGroup;
            if (mEventInstance.getChannelGroup(out channelGroup) != FMOD.RESULT.OK)
                return;

            if (!channelGroup.hasHandle())
                return;

            int numDSPs;
            if (channelGroup.getNumDSPs(out numDSPs) != FMOD.RESULT.OK)
                return;

            for (var i = 0; i < numDSPs; ++i)
            {
                FMOD.DSP dsp;
                if (channelGroup.getDSP(i, out dsp) != FMOD.RESULT.OK)
                    continue;

                if (!dsp.hasHandle())
                    continue;

                var dspName = "";
                var dspVersion = 0u;
                var dspNumChannels = 0;
                var dspConfigWidth = 0;
                var dspConfigHeight = 0;
                if (dsp.getInfo(out dspName, out dspVersion, out dspNumChannels, out dspConfigWidth, out dspConfigHeight) != FMOD.RESULT.OK)
                    continue;

                if (dspName == "Steam Audio Spatializer")
                {
                    mDSP = dsp;
                    mFoundDSP = true;
                    return;
                }
            }
        }
    }
}

#endif
