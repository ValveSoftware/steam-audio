//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Reflection;
using UnityEngine;

namespace SteamAudio
{
    public sealed class FMODAudioEngineSource : AudioEngineSource
    {
        public override void Initialize(GameObject gameObject)
        {
            FindDSP(gameObject);
        }

        void BindToFMODStudioPlugin()
        {
            var assemblySuffix = ",Assembly-CSharp-firstpass";

            FMOD_DSP = Type.GetType("FMOD.DSP" + assemblySuffix);
            FMOD_ChannelGroup = Type.GetType("FMOD.ChannelGroup" + assemblySuffix);
            FMOD_Studio_EventInstance = Type.GetType("FMOD.Studio.EventInstance" + assemblySuffix);
            FMODUnity_StudioEventEmitter = Type.GetType("FMODUnity.StudioEventEmitter" + assemblySuffix);

            FMODUnity_StudioEventEmitter_EventInstance = FMODUnity_StudioEventEmitter.GetProperty("EventInstance");

            FMOD_Studio_EventInstance_getChannelGroup = FMOD_Studio_EventInstance.GetMethod("getChannelGroup");
            FMOD_ChannelGroup_getNumDSPs = FMOD_ChannelGroup.GetMethod("getNumDSPs");
            FMOD_ChannelGroup_getDSP = FMOD_ChannelGroup.GetMethod("getDSP");
            FMOD_DSP_getParameterBool = FMOD_DSP.GetMethod("getParameterBool");
            FMOD_DSP_setParameterData = FMOD_DSP.GetMethod("setParameterData");

            var candidates = FMOD_DSP.GetMethods();
            foreach (var candidate in candidates)
            {
                if (candidate.Name == "getInfo" && candidate.GetParameters().Length == 5)
                {
                    FMOD_DSP_getInfo = candidate;
                    break;
                }
            }
        }

        void FindDSP(GameObject gameObject)
        {
            if (foundDSP)
                return;

            BindToFMODStudioPlugin();

            var eventEmitter = gameObject.GetComponent(FMODUnity_StudioEventEmitter) as object;
            if (eventEmitter == null)
                return;

            var eventInstance = FMODUnity_StudioEventEmitter_EventInstance.GetValue(eventEmitter, null);

            var channelGroup = Activator.CreateInstance(FMOD_ChannelGroup);

            var getChannelGroupArgs = new object[] { channelGroup };
            FMOD_Studio_EventInstance_getChannelGroup.Invoke(eventInstance, getChannelGroupArgs);
            channelGroup = getChannelGroupArgs[0];

            var getNumDSPsArgs = new object[] { 0 };
            FMOD_ChannelGroup_getNumDSPs.Invoke(channelGroup, getNumDSPsArgs);
            int numDSPs = (int)getNumDSPsArgs[0];

            for (var i = 0; i < numDSPs; ++i)
            {
                var getDSPArgs = new object[] { i, dsp };
                FMOD_ChannelGroup_getDSP.Invoke(channelGroup, getDSPArgs);
                dsp = getDSPArgs[1];

                var dspName = "";
                var dspVersion = 0u;
                var dspNumChannels = 0;
                var dspConfigWidth = 0;
                var dspConfigHeight = 0;

                var getInfoArgs = new object[] { dspName, dspVersion, dspNumChannels, dspConfigWidth, dspConfigHeight };
                FMOD_DSP_getInfo.Invoke(dsp, getInfoArgs);
                dspName = (string)getInfoArgs[0];

                if (dspName == "Steam Audio Spatializer")
                {
                    foundDSP = true;
                    return;
                }
            }
        }

        public override void Destroy()
        {
            foundDSP = false;
        }

        public override bool UsesBakedStaticListener(SteamAudioSource steamAudioSource)
        {
            FindDSP(steamAudioSource.gameObject);
            if (!foundDSP)
                return false;

            var usesBakedStaticListener = false;

            var getParameterBoolArgs = new object[] { 12, usesBakedStaticListener };
            FMOD_DSP_getParameterBool.Invoke(dsp, getParameterBoolArgs);
            usesBakedStaticListener = (bool) getParameterBoolArgs[1];

            return usesBakedStaticListener;
        }

        public override void SendIdentifier(SteamAudioSource steamAudioSource, int identifier)
        {
            FindDSP(steamAudioSource.gameObject);
            if (!foundDSP)
                return;

            FMOD_DSP_setParameterData.Invoke(dsp, new object[] { 13, BitConverter.GetBytes(identifier) });
        }

        bool foundDSP = false;
        object dsp = null;

        Type FMOD_DSP;
        Type FMOD_ChannelGroup;
        Type FMOD_Studio_EventInstance;
        Type FMODUnity_StudioEventEmitter;

        PropertyInfo FMODUnity_StudioEventEmitter_EventInstance;

        MethodInfo FMOD_Studio_EventInstance_getChannelGroup;
        MethodInfo FMOD_ChannelGroup_getNumDSPs;
        MethodInfo FMOD_ChannelGroup_getDSP;
        MethodInfo FMOD_DSP_getInfo;
        MethodInfo FMOD_DSP_getParameterBool;
        MethodInfo FMOD_DSP_setParameterData;
    }
}