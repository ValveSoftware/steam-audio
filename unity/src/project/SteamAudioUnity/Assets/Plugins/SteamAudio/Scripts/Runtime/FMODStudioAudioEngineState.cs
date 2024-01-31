//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//
#if STEAMAUDIO_ENABLED

using System;
using System.Reflection;
using UnityEngine;

namespace SteamAudio
{
    public sealed class FMODStudioAudioEngineState : AudioEngineState
    {
        public override void Initialize(IntPtr context, IntPtr defaultHRTF, SimulationSettings simulationSettings, PerspectiveCorrection correction)
        {
            FMODStudioAPI.iplFMODInitialize(context);
            FMODStudioAPI.iplFMODSetHRTF(defaultHRTF);
            FMODStudioAPI.iplFMODSetSimulationSettings(simulationSettings);
        }

        public override void Destroy()
        {
            FMODStudioAPI.iplFMODTerminate();
        }

        public override void SetHRTF(IntPtr hrtf)
        {
            FMODStudioAPI.iplFMODSetHRTF(hrtf);
        }

        public override void SetReverbSource(Source reverbSource)
        {
            FMODStudioAPI.iplFMODSetReverbSource(reverbSource.Get());
        }
    }

    public sealed class FMODStudioAudioEngineStateHelpers : AudioEngineStateHelpers
    {
        static bool mBoundToPlugin = false;

        static Type FMOD_SPEAKERMODE;
        static Type FMOD_System;
        static Type FMODUnity_StudioListener;
        static Type FMODUnity_RuntimeManager;
        static PropertyInfo FMODUnity_RuntimeManager_CoreSystem;
        static MethodInfo FMOD_System_getSoftwareFormat;
        static MethodInfo FMOD_System_getDSPBufferSize;

        public override Transform GetListenerTransform()
        {
            BindToFMODStudioPlugin();

            var fmodStudioListener = (MonoBehaviour) GameObject.FindObjectOfType(FMODUnity_StudioListener);
            return (fmodStudioListener != null) ? fmodStudioListener.transform : null;
        }

        public override AudioSettings GetAudioSettings()
        {
            BindToFMODStudioPlugin();

            var audioSettings = new AudioSettings { };

            var fmodSystem = FMODUnity_RuntimeManager_CoreSystem.GetValue(null, null);

            var speakerMode = Activator.CreateInstance(FMOD_SPEAKERMODE);
            var getSoftwareFormatArgs = new object[] { 0, speakerMode, 0 };
            FMOD_System_getSoftwareFormat.Invoke(fmodSystem, getSoftwareFormatArgs);
            audioSettings.samplingRate = (int) getSoftwareFormatArgs[0];

            var getDSPBufferSizeArgs = new object[] { 0u, 0 };
            FMOD_System_getDSPBufferSize.Invoke(fmodSystem, getDSPBufferSizeArgs);
            audioSettings.frameSize = (int) ((uint) getDSPBufferSizeArgs[0]);

            return audioSettings;
        }

        static void BindToFMODStudioPlugin()
        {
            if (mBoundToPlugin)
                return;

            var assemblySuffix = ",FMODUnity";

            FMOD_SPEAKERMODE = Type.GetType("FMOD.SPEAKERMODE" + assemblySuffix);
            FMOD_System = Type.GetType("FMOD.System" + assemblySuffix);
            FMODUnity_StudioListener = Type.GetType("FMODUnity.StudioListener" + assemblySuffix);
            FMODUnity_RuntimeManager = Type.GetType("FMODUnity.RuntimeManager" + assemblySuffix);

            FMODUnity_RuntimeManager_CoreSystem = FMODUnity_RuntimeManager.GetProperty("CoreSystem");

            FMOD_System_getSoftwareFormat = FMOD_System.GetMethod("getSoftwareFormat");
            FMOD_System_getDSPBufferSize = FMOD_System.GetMethod("getDSPBufferSize");
        }
    }
}

#endif