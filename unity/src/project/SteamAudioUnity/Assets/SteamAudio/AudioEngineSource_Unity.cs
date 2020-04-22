//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;

namespace SteamAudio
{
    struct UnityAudioSourceData
    {
        public AudioRolloffMode rolloffMode;
        public float minDistance;
        public float maxDistance;
        public AnimationCurve distanceAttenuationCurve;

        public static bool AreEqual(UnityAudioSourceData lhs,
                                    UnityAudioSourceData rhs)
        {
            if (lhs.rolloffMode != rhs.rolloffMode)
                return false;
            if (lhs.minDistance != rhs.minDistance)
                return false;
            if (lhs.maxDistance != rhs.maxDistance)
                return false;
            if (lhs.distanceAttenuationCurve == null ^ rhs.distanceAttenuationCurve == null)
                return false;

            if (lhs.distanceAttenuationCurve != null && rhs.distanceAttenuationCurve != null) 
            {
                AnimationCurve lhsCurve = lhs.distanceAttenuationCurve;
                AnimationCurve rhsCurve = rhs.distanceAttenuationCurve;

                if (lhsCurve.length != rhsCurve.length)
                    return false;
                if (lhsCurve.preWrapMode != rhsCurve.preWrapMode)
                    return false;
                if (lhsCurve.postWrapMode != rhsCurve.postWrapMode)
                    return false;

                for (int i = 0; i < lhsCurve.length; ++i) 
                {
                    Keyframe lhsKey = lhsCurve.keys[i];
                    Keyframe rhsKey = rhsCurve.keys[i];

                    if (lhsKey.inTangent != rhsKey.inTangent)
                        return false;
                    if (lhsKey.outTangent != rhsKey.outTangent)
                        return false;
                    if (lhsKey.time != rhsKey.time)
                        return false;
                    if (lhsKey.value != rhsKey.value)
                        return false;
#if UNITY_2018_1_OR_NEWER
                    if (lhsKey.inWeight != rhsKey.inWeight)
                        return false;
                    if (lhsKey.outWeight != rhsKey.outWeight)
                        return false;
                    if (lhsKey.weightedMode != rhsKey.weightedMode)
                        return false;
#else
                    if (lhsKey.tangentMode != rhsKey.tangentMode)
                        return false;
#endif
                }
            }

            return true;
        }
    }

    public sealed class UnityAudioEngineSource : AudioEngineSource
    {
        public override void Initialize(GameObject gameObject)
        {
            audioSource = gameObject.GetComponent<AudioSource>();
            thisObject = GCHandle.Alloc(this);
        }

        public override void Destroy()
        {
            thisObject.Free();
        }

        public override void GetParameters(SteamAudioSource steamAudioSource)
        {
            // todo: can we copy the curve only if it's changed?
            m_curAudioSourceData.rolloffMode = audioSource.rolloffMode;
            m_curAudioSourceData.minDistance = audioSource.minDistance;
            m_curAudioSourceData.maxDistance = audioSource.maxDistance;
            m_curAudioSourceData.distanceAttenuationCurve = audioSource.GetCustomCurve(AudioSourceCurveType.CustomRolloff);

            // todo: ideally, we would check if the curve has been edited by the user, and set the dirty flag accordingly.
            // however, since we would need to know when the c api has consumed the distance attenuation model struct
            // internally, which would require information to be passed across several layers of code, we opt to instead
            // always treat the distance attenuation model as dirty when running in the editor, and never when running
            // in a player. once the indirect calculation is moved to the unity side, it will be easier to do this the
            // right way.
            bool audioSourceDataChanged = (Application.isEditor);

            m_prevAudioSourceData = m_curAudioSourceData;

            if (steamAudioSource.physicsBasedAttenuation) 
            {
                steamAudioSource.distanceAttenuationModel.type = DistanceAttenuationModelType.Default;
            } 
            else 
            {
                steamAudioSource.distanceAttenuationModel.type = DistanceAttenuationModelType.Callback;
                steamAudioSource.distanceAttenuationModel.callback = DistanceAttenuationCallback;
                steamAudioSource.distanceAttenuationModel.userData = GCHandle.ToIntPtr(thisObject);
                steamAudioSource.distanceAttenuationModel.dirty = (audioSourceDataChanged) ? Bool.True : Bool.False;
            }

            steamAudioSource.airAbsorptionModel.type = AirAbsorptionModelType.Default;
        }

        public override void UpdateParameters(SteamAudioSource steamAudioSource)
        {
            if (!audioSource)
                return;

            var directBinauralValue = (steamAudioSource.directBinaural) ? 1.0f : 0.0f;
            var hrtfInterpolationValue = (float)steamAudioSource.interpolation;
            var distanceAttenuationValue = (steamAudioSource.physicsBasedAttenuation) ? 1.0f : 0.0f;
            var airAbsorptionValue = (steamAudioSource.airAbsorption) ? 1.0f : 0.0f;
            var dipoleWeightValue = steamAudioSource.dipoleWeight;
            var dipolePowerValue = steamAudioSource.dipolePower;
            var occlusionModeValue = (float)steamAudioSource.occlusionMode;
            var occlusionMethodValue = (float)steamAudioSource.occlusionMethod;
            var sourceRadiusValue = steamAudioSource.sourceRadius;
            var directMixLevelValue = steamAudioSource.directMixLevel;
            var reflectionsValue = (steamAudioSource.reflections) ? 1.0f : 0.0f;
            var indirectBinauralValue = (steamAudioSource.indirectBinaural) ? 1.0f : 0.0f;
            var indirectMixLevelValue = steamAudioSource.indirectMixLevel;
            var simulationTypeValue = (steamAudioSource.simulationType == SourceSimulationType.Realtime) ? 0.0f : 1.0f;
            var usesStaticListenerValue = (steamAudioSource.simulationType == SourceSimulationType.BakedStaticListener) ? 1.0f : 0.0f;
            var bypassDuringInitValue = (steamAudioSource.avoidSilenceDuringInit) ? 1.0f : 0.0f;
            var hrtfIndexValue = (float) steamAudioSource.hrtfIndex;
            var overrideHRTFIndexValue = (steamAudioSource.overrideHRTFIndex) ? 1.0f : 0.0f;

            audioSource.SetSpatializerFloat(0, directBinauralValue);
            audioSource.SetSpatializerFloat(1, hrtfInterpolationValue);
            audioSource.SetSpatializerFloat(2, distanceAttenuationValue);
            audioSource.SetSpatializerFloat(3, airAbsorptionValue);
            audioSource.SetSpatializerFloat(4, occlusionModeValue);
            audioSource.SetSpatializerFloat(5, occlusionMethodValue);
            audioSource.SetSpatializerFloat(6, sourceRadiusValue);
            audioSource.SetSpatializerFloat(7, directMixLevelValue);
            audioSource.SetSpatializerFloat(8, reflectionsValue);
            audioSource.SetSpatializerFloat(9, indirectBinauralValue);
            audioSource.SetSpatializerFloat(10, indirectMixLevelValue);
            audioSource.SetSpatializerFloat(11, simulationTypeValue);
            audioSource.SetSpatializerFloat(12, usesStaticListenerValue);
            audioSource.SetSpatializerFloat(14, bypassDuringInitValue);
            audioSource.SetSpatializerFloat(15, dipoleWeightValue);
            audioSource.SetSpatializerFloat(16, dipolePowerValue);
            audioSource.SetSpatializerFloat(17, hrtfIndexValue);
            audioSource.SetSpatializerFloat(18, overrideHRTFIndexValue);

            var index = 19;
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.distanceAttenuation);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.airAbsorptionLow);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.airAbsorptionMid);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.airAbsorptionHigh);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.propagationDelay);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.occlusionFactor);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.transmissionFactorLow);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.transmissionFactorMid);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.transmissionFactorHigh);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.directivityFactor);

            var distanceAttenuationCallback = IntPtr.Zero;
            if (steamAudioSource.distanceAttenuationModel.callback != null) 
            {
                distanceAttenuationCallback = Marshal.GetFunctionPointerForDelegate(steamAudioSource.distanceAttenuationModel.callback);
            }
            var distanceAttenuationUserData = steamAudioSource.distanceAttenuationModel.userData;

            if (steamAudioSource.physicsBasedAttenuationForIndirect) 
            {
                audioSource.SetSpatializerFloat(index++, (float) DistanceAttenuationModelType.Default);
            } 
            else 
            {
                audioSource.SetSpatializerFloat(index++, (float) steamAudioSource.distanceAttenuationModel.type);
            }

            audioSource.SetSpatializerFloat(index++, steamAudioSource.distanceAttenuationModel.minDistance);

            if (IntPtr.Size == sizeof(Int64)) 
            {
                // note: assumes little-endian
                var valueBytes = BitConverter.GetBytes(distanceAttenuationCallback.ToInt64());
                var valueLow = BitConverter.ToSingle(valueBytes, 0);
                var valueHigh = BitConverter.ToSingle(valueBytes, 4);
                audioSource.SetSpatializerFloat(index++, valueLow);
                audioSource.SetSpatializerFloat(index++, valueHigh);

                valueBytes = BitConverter.GetBytes(distanceAttenuationUserData.ToInt64());
                valueLow = BitConverter.ToSingle(valueBytes, 0);
                valueHigh = BitConverter.ToSingle(valueBytes, 4);
                audioSource.SetSpatializerFloat(index++, valueLow);
                audioSource.SetSpatializerFloat(index++, valueHigh);
            } 
            else 
            {
                var valueBytes = BitConverter.GetBytes(distanceAttenuationCallback.ToInt32());
                var valueLow = BitConverter.ToSingle(valueBytes, 0);
                audioSource.SetSpatializerFloat(index++, valueLow);
                audioSource.SetSpatializerFloat(index++, 0.0f);

                valueBytes = BitConverter.GetBytes(distanceAttenuationUserData.ToInt32());
                valueLow = BitConverter.ToSingle(valueBytes, 0);
                audioSource.SetSpatializerFloat(index++, valueLow);
                audioSource.SetSpatializerFloat(index++, 0.0f);
            }

            audioSource.SetSpatializerFloat(index++, (steamAudioSource.distanceAttenuationModel.dirty == Bool.True) ? 1.0f : 0.0f);
        }

        public override bool ShouldSendIdentifier(SteamAudioSource steamAudioSource)
        {
            return (steamAudioSource.simulationType == SourceSimulationType.BakedStaticSource ||
                steamAudioSource.simulationType == SourceSimulationType.BakedStaticListener);
        }

        public override bool UsesBakedStaticListener(SteamAudioSource steamAudioSource)
        {
            return (steamAudioSource.simulationType == SourceSimulationType.BakedStaticListener);
        }

        public override void SendIdentifier(SteamAudioSource steamAudioSource, int identifier)
        {
            var identifierFloat = BitConverter.ToSingle(BitConverter.GetBytes(identifier), 0);
            audioSource.SetSpatializerFloat(13, identifierFloat);
        }

        [MonoPInvokeCallback(typeof(DistanceAttenuationCallback))]
        public static float DistanceAttenuationCallback(float distance, IntPtr userData)
        {
            var thisObject = (UnityAudioEngineSource) GCHandle.FromIntPtr(userData).Target;

            var rMin = thisObject.m_curAudioSourceData.minDistance;
            var rMax = thisObject.m_curAudioSourceData.maxDistance;

            switch (thisObject.m_curAudioSourceData.rolloffMode) 
            {
                case AudioRolloffMode.Logarithmic:
                    if (distance < rMin)
                        return 1.0f;
                    else if (distance > rMax)
                        return 0.0f;
                    else
                        return rMin / distance;
                case AudioRolloffMode.Linear:
                    if (distance < rMin)
                        return 1.0f;
                    else if (distance > rMax)
                        return 0.0f;
                    else
                        return (rMax - distance) / (rMax - rMin);
                case AudioRolloffMode.Custom:
#if UNITY_2018_1_OR_NEWER
                    return thisObject.m_curAudioSourceData.distanceAttenuationCurve.Evaluate(distance / rMax);
#else
                    if (distance < rMin)
                        return 1.0f;
                    else if (distance > rMax)
                        return 0.0f;
                    else
                        return rMin / distance;
#endif
                default:
                    return 0.0f;
            }
        }

        AudioSource     audioSource     = null;

        GCHandle thisObject;

        UnityAudioSourceData m_curAudioSourceData = new UnityAudioSourceData();
        UnityAudioSourceData m_prevAudioSourceData = new UnityAudioSourceData();
    }
}