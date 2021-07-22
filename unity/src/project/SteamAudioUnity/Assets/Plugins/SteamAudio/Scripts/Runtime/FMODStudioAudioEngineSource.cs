//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEngine;

namespace SteamAudio
{
    public sealed class FMODStudioAudioEngineSource : AudioEngineSource
    {
        bool mFoundDSP = false;
        object mDSP = null;
        byte[] mSimulationOutputs = null;

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
        static MethodInfo FMOD_DSP_setParameterData;
        static bool mBoundToPlugin = false;

        const int kSimulationOutputsParamIndex = 30;

        public override void Initialize(GameObject gameObject)
        {
            FindDSP(gameObject);

            mSimulationOutputs = new byte[IntPtr.Size];
        }

        public override void Destroy()
        {
            mFoundDSP = false;
        }

        public override void UpdateParameters(SteamAudioSource source)
        {
            FindDSP(source.gameObject);
            if (!mFoundDSP)
                return;

            var ptr = source.GetSource().Get();

            if (IntPtr.Size == sizeof(Int64))
            {
                var ptrValue = ptr.ToInt64();

                mSimulationOutputs[0] = (byte) (ptrValue >> 0);
                mSimulationOutputs[1] = (byte) (ptrValue >> 8);
                mSimulationOutputs[2] = (byte) (ptrValue >> 16);
                mSimulationOutputs[3] = (byte) (ptrValue >> 24);
                mSimulationOutputs[4] = (byte) (ptrValue >> 32);
                mSimulationOutputs[5] = (byte) (ptrValue >> 40);
                mSimulationOutputs[6] = (byte) (ptrValue >> 48);
                mSimulationOutputs[7] = (byte) (ptrValue >> 56);
            }
            else
            {
                var ptrValue = ptr.ToInt32();

                mSimulationOutputs[0] = (byte) (ptrValue >> 0);
                mSimulationOutputs[1] = (byte) (ptrValue >> 8);
                mSimulationOutputs[2] = (byte) (ptrValue >> 16);
                mSimulationOutputs[3] = (byte) (ptrValue >> 24);
            }

            var index = kSimulationOutputsParamIndex;
            FMOD_DSP_setParameterData.Invoke(mDSP, new object[] { index++, mSimulationOutputs });
        }

        void FindDSP(GameObject gameObject)
        {
            if (mFoundDSP)
                return;

            BindToFMODStudioPlugin();

            var eventEmitter = gameObject.GetComponent(FMODUnity_StudioEventEmitter) as object;
            if (eventEmitter == null)
                return;

            var eventInstance = FMODUnity_StudioEventEmitter_EventInstance.GetValue(eventEmitter, null);
            if (!((bool)FMOD_Studio_EventInstance_isValid.Invoke(eventInstance, null)))
                return;

            var channelGroup = Activator.CreateInstance(FMOD_ChannelGroup);

            var getChannelGroupArgs = new object[] { channelGroup };
            FMOD_Studio_EventInstance_getChannelGroup.Invoke(eventInstance, getChannelGroupArgs);
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

            mBoundToPlugin = true;
        }
    }
}
