//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Reflection;
using UnityEngine;

namespace SteamAudio
{
    public sealed class FMODStudioAudioEngineSource : AudioEngineSource
    {
        bool mFoundDSP = false;
        object mEventEmitter = null;
        object mEventInstance = null;
        object mDSP = null;
        SteamAudioSource mSteamAudioSource = null;
        int mHandle = -1;

        static Type FMOD_DSP;
        static Type FMOD_ChannelGroup;
        static Type FMOD_Studio_EventInstance;
        static Type FMODUnity_StudioEventEmitter;
        static PropertyInfo FMODUnity_StudioEventEmitter_EventInstance;
        static MethodInfo FMOD_Studio_EventInstance_getChannelGroup;
        static MethodInfo FMOD_Studio_EventInstance_isValid;
        static MethodInfo FMOD_ChannelGroup_getNumDSPs;
        static MethodInfo FMOD_ChannelGroup_getDSP;
        static MethodInfo FMOD_DSP_getInfo;
        static MethodInfo FMOD_DSP_setParameterInt;
        static bool mBoundToPlugin = false;

        const int kSimulationOutputsParamIndex = 33;

        public override void Initialize(GameObject gameObject)
        {
            FindDSP(gameObject);

            mSteamAudioSource = gameObject.GetComponent<SteamAudioSource>();
            if (mSteamAudioSource)
            {
                mHandle = API.iplFMODAddSource(mSteamAudioSource.GetSource().Get());
            }
        }

        public override void Destroy()
        {
            mFoundDSP = false;

            if (mSteamAudioSource)
            {
                API.iplFMODRemoveSource(mHandle);
            }
        }

        public override void UpdateParameters(SteamAudioSource source)
        {
            CheckForChangedEventInstance();

            FindDSP(source.gameObject);
            if (!mFoundDSP)
                return;

            var index = kSimulationOutputsParamIndex;
            FMOD_DSP_setParameterInt.Invoke(mDSP, new object[] { index++, mHandle });
        }

        void CheckForChangedEventInstance()
        {
            if (mEventEmitter != null)
            {
                var eventInstance = FMODUnity_StudioEventEmitter_EventInstance.GetValue(mEventEmitter, null);
                if (eventInstance != mEventInstance)
                {
                    // The event instance is different from the one we last used, which most likely means the
                    // event-related objects were destroyed and re-created. Make sure we look for the DSP instance
                    // when FindDSP is called next.
                    mFoundDSP = false;
                }
            } 
            else
            {
                // We haven't yet seen a valid event emitter component, so make sure we look for one when
                // FindDSP is called.
                mFoundDSP = false;
            }
        }

        void FindDSP(GameObject gameObject)
        {
            if (mFoundDSP)
                return;

            BindToFMODStudioPlugin();

            mEventEmitter = gameObject.GetComponent(FMODUnity_StudioEventEmitter) as object;
            if (mEventEmitter == null)
                return;

            mEventInstance = FMODUnity_StudioEventEmitter_EventInstance.GetValue(mEventEmitter, null);
            if (!((bool)FMOD_Studio_EventInstance_isValid.Invoke(mEventInstance, null)))
                return;

            var channelGroup = Activator.CreateInstance(FMOD_ChannelGroup);

            var getChannelGroupArgs = new object[] { channelGroup };
            FMOD_Studio_EventInstance_getChannelGroup.Invoke(mEventInstance, getChannelGroupArgs);
            channelGroup = getChannelGroupArgs[0];

            var getNumDSPsArgs = new object[] { 0 };
            FMOD_ChannelGroup_getNumDSPs.Invoke(channelGroup, getNumDSPsArgs);
            int numDSPs = (int)getNumDSPsArgs[0];

            for (var i = 0; i < numDSPs; ++i)
            {
                var getDSPArgs = new object[] { i, mDSP };
                FMOD_ChannelGroup_getDSP.Invoke(channelGroup, getDSPArgs);
                mDSP = getDSPArgs[1];

                var dspName = "";
                var dspVersion = 0u;
                var dspNumChannels = 0;
                var dspConfigWidth = 0;
                var dspConfigHeight = 0;

                var getInfoArgs = new object[] { dspName, dspVersion, dspNumChannels, dspConfigWidth, dspConfigHeight };
                FMOD_DSP_getInfo.Invoke(mDSP, getInfoArgs);
                dspName = (string)getInfoArgs[0];

                if (dspName == "Steam Audio Spatializer")
                {
                    mFoundDSP = true;
                    return;
                }
            }
        }

        static void BindToFMODStudioPlugin()
        {
            if (mBoundToPlugin)
                return;

            var assemblySuffix = ",FMODUnity";

            FMOD_DSP = Type.GetType("FMOD.DSP" + assemblySuffix);
            FMOD_ChannelGroup = Type.GetType("FMOD.ChannelGroup" + assemblySuffix);
            FMOD_Studio_EventInstance = Type.GetType("FMOD.Studio.EventInstance" + assemblySuffix);
            FMODUnity_StudioEventEmitter = Type.GetType("FMODUnity.StudioEventEmitter" + assemblySuffix);

            FMODUnity_StudioEventEmitter_EventInstance = FMODUnity_StudioEventEmitter.GetProperty("EventInstance");

            FMOD_Studio_EventInstance_getChannelGroup = FMOD_Studio_EventInstance.GetMethod("getChannelGroup");
            FMOD_Studio_EventInstance_isValid = FMOD_Studio_EventInstance.GetMethod("isValid");
            FMOD_ChannelGroup_getNumDSPs = FMOD_ChannelGroup.GetMethod("getNumDSPs");
            FMOD_ChannelGroup_getDSP = FMOD_ChannelGroup.GetMethod("getDSP");
            FMOD_DSP_setParameterInt = FMOD_DSP.GetMethod("setParameterInt");

            var candidates = FMOD_DSP.GetMethods();
            foreach (var candidate in candidates)
            {
                if (candidate.Name == "getInfo" && candidate.GetParameters().Length == 5)
                {
                    FMOD_DSP_getInfo = candidate;
                    break;
                }
            }

            mBoundToPlugin = true;
        }
    }
}
