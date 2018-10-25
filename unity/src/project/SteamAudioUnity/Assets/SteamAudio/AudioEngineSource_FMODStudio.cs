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
        private const int k_occlusionModeIndex = 4;
        private const int k_occlusionMethodIndex = 5;
        private const int k_sourceRadiusIndex = 6;
        private const int k_enableIndirectIndex = 8;
        private const int k_useBakedStaticListenerIndex = 12;
        private const int k_setIdentifierIndex = 13;
        private const int k_dipoleWeightIndex = 15;
        private const int k_dipolePowerIndex = 16;
        private const int k_directPathIndexStart = 19;

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
            FMOD_Studio_EventInstance_isValid = FMOD_Studio_EventInstance.GetMethod("isValid");
            FMOD_ChannelGroup_getNumDSPs = FMOD_ChannelGroup.GetMethod("getNumDSPs");
            FMOD_ChannelGroup_getDSP = FMOD_ChannelGroup.GetMethod("getDSP");
            FMOD_DSP_getParameterBool = FMOD_DSP.GetMethod("getParameterBool");
            FMOD_DSP_getParameterFloat = FMOD_DSP.GetMethod("getParameterFloat");
            FMOD_DSP_getParameterInt = FMOD_DSP.GetMethod("getParameterInt");
            FMOD_DSP_setParameterData = FMOD_DSP.GetMethod("setParameterData");
            FMOD_DSP_setParameterFloat = FMOD_DSP.GetMethod("setParameterFloat");

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
            if (!((bool) FMOD_Studio_EventInstance_isValid.Invoke(eventInstance, null)))
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

        public override void UpdateParameters(SteamAudioSource steamAudioSource)
        {
            FindDSP(steamAudioSource.gameObject);
            if (!foundDSP)
                return;

            // update the values of the directPath in Fmod
            var index = k_directPathIndexStart;	// this is where the directPath settings begin

            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.distanceAttenuation });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.airAbsorptionLow });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.airAbsorptionMid });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.airAbsorptionHigh });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.propagationDelay });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.occlusionFactor });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.transmissionFactorLow });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.transmissionFactorMid });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.transmissionFactorHigh });
            FMOD_DSP_setParameterFloat.Invoke(dsp, new object[] { index++, steamAudioSource.directPath.directivityFactor });
        }

        public override void GetParameters(SteamAudioSource steamAudioSource)
        {
            FindDSP(steamAudioSource.gameObject);
            if (!foundDSP)
                return;

            // get values set in Fmod and pass to AudioEngineSource so it can do the audibility checks
            {
                var getParameterFloatArgs = new object[] { k_dipoleWeightIndex, 0.0f };
                FMOD_DSP_getParameterFloat.Invoke(dsp, getParameterFloatArgs);
                steamAudioSource.dipoleWeight = (float)getParameterFloatArgs[1];

                getParameterFloatArgs[0] = k_dipolePowerIndex;
                FMOD_DSP_getParameterFloat.Invoke(dsp, getParameterFloatArgs);
                steamAudioSource.dipolePower = (float)getParameterFloatArgs[1];

                getParameterFloatArgs[0] = k_sourceRadiusIndex;
                FMOD_DSP_getParameterFloat.Invoke(dsp, getParameterFloatArgs);
                steamAudioSource.sourceRadius = (float)getParameterFloatArgs[1];
            }

            {
                var getParameterIntArgs = new object[] { k_occlusionModeIndex, 0 };
                FMOD_DSP_getParameterInt.Invoke(dsp, getParameterIntArgs);
                steamAudioSource.occlusionMode = (OcclusionMode)getParameterIntArgs[1];

                getParameterIntArgs[0] = k_occlusionMethodIndex;
                FMOD_DSP_getParameterInt.Invoke(dsp, getParameterIntArgs);
                steamAudioSource.occlusionMethod = (OcclusionMethod)getParameterIntArgs[1];
            }

            {
                var getParameterBoolArgs = new object[] { k_enableIndirectIndex, false };
                FMOD_DSP_getParameterBool.Invoke(dsp, getParameterBoolArgs);
                steamAudioSource.reflections = (bool)getParameterBoolArgs[1];
            }
        }

        public override bool UsesBakedStaticListener(SteamAudioSource steamAudioSource)
        {
            FindDSP(steamAudioSource.gameObject);
            if (!foundDSP)
                return false;

            var usesBakedStaticListener = false;

            var getParameterBoolArgs = new object[] { k_useBakedStaticListenerIndex, usesBakedStaticListener };
            FMOD_DSP_getParameterBool.Invoke(dsp, getParameterBoolArgs);
            usesBakedStaticListener = (bool) getParameterBoolArgs[1];

            return usesBakedStaticListener;
        }

        public override void SendIdentifier(SteamAudioSource steamAudioSource, int identifier)
        {
            FindDSP(steamAudioSource.gameObject);
            if (!foundDSP)
                return;

            FMOD_DSP_setParameterData.Invoke(dsp, new object[] { k_setIdentifierIndex, BitConverter.GetBytes(identifier) });
        }

        bool foundDSP = false;
        object dsp = null;

        Type FMOD_DSP;
        Type FMOD_ChannelGroup;
        Type FMOD_Studio_EventInstance;
        Type FMODUnity_StudioEventEmitter;

        PropertyInfo FMODUnity_StudioEventEmitter_EventInstance;

        MethodInfo FMOD_Studio_EventInstance_getChannelGroup;
        MethodInfo FMOD_Studio_EventInstance_isValid;
        MethodInfo FMOD_ChannelGroup_getNumDSPs;
        MethodInfo FMOD_ChannelGroup_getDSP;
        MethodInfo FMOD_DSP_getInfo;
        MethodInfo FMOD_DSP_getParameterBool;
        MethodInfo FMOD_DSP_getParameterFloat;
        MethodInfo FMOD_DSP_getParameterInt;
        MethodInfo FMOD_DSP_setParameterData;
        MethodInfo FMOD_DSP_setParameterFloat;
    }
}