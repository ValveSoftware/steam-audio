//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public enum GameEngineStateInitReason
    {
        ExportingScene,
        GeneratingProbes,
        EditingProbes,
        Baking,
        Playing
    }

    public class GameEngineState
    {
        public void Initialize(SimulationSettingsValue settings, ComponentCache componentCache,
            GameEngineStateInitReason reason)
        {
            PhononCore.iplCreateContext(LogMessage, IntPtr.Zero, IntPtr.Zero, ref context);

            if (reason != GameEngineStateInitReason.EditingProbes)
            {
                var customSettings = componentCache.SteamAudioCustomSettings();

                var useOpenCL = false;
                var computeDeviceType = ComputeDeviceType.Any;
                var requiresTan = false;
                var minReservableCUs = 0;
                var maxCUsToReserve = 0;

                if (customSettings)
                {
#if ((UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN) && UNITY_64)
                    convolutionType = customSettings.convolutionOption;

                    if (customSettings.convolutionOption == ConvolutionOption.TrueAudioNext)
                    {
                        useOpenCL = true;
                        requiresTan = true;
                        computeDeviceType = ComputeDeviceType.GPU;
                        minReservableCUs = customSettings.minComputeUnitsToReserve;
                        maxCUsToReserve = customSettings.maxComputeUnitsToReserve;
                    }
                    else if (customSettings.rayTracerOption == SceneType.RadeonRays)
                    {
                        useOpenCL = true;
                    }
#else
                    convolutionType = ConvolutionOption.Phonon;
                    customSettings.convolutionOption = ConvolutionOption.Phonon;
#endif
                }

                try
                {
                    var deviceFilter = new ComputeDeviceFilter
                    {
                        type = computeDeviceType,
                        requiresTrueAudioNext = (requiresTan) ? Bool.True : Bool.False,
                        minReservableCUs = minReservableCUs,
                        maxCUsToReserve = maxCUsToReserve
                    };

                    computeDevice.Create(context, useOpenCL, deviceFilter);
                }
                catch (Exception e)
                {
                    Debug.LogWarning(String.Format("Unable to initialize TrueAudio Next: {0}. Using Phonon convolution.",
                        e.Message));

                    convolutionType = ConvolutionOption.Phonon;
                    customSettings.convolutionOption = ConvolutionOption.Phonon;
                }

                var inEditor = !SteamAudioManager.IsAudioEngineInitializing();

                simulationSettings = new SimulationSettings
                {
                    sceneType               = (customSettings) ? customSettings.rayTracerOption : SceneType.Phonon,
                    rays                    = (inEditor) ? settings.BakeRays : settings.RealtimeRays,
                    secondaryRays           = (inEditor) ? settings.BakeSecondaryRays : settings.RealtimeSecondaryRays,
                    bounces                 = (inEditor) ? settings.BakeBounces : settings.RealtimeBounces,
                    irDuration              = (customSettings && customSettings.convolutionOption == ConvolutionOption.TrueAudioNext) ? customSettings.Duration : settings.Duration,
                    ambisonicsOrder         = (customSettings && customSettings.convolutionOption == ConvolutionOption.TrueAudioNext) ? customSettings.AmbisonicsOrder : settings.AmbisonicsOrder,
                    maxConvolutionSources   = (customSettings && customSettings.convolutionOption == ConvolutionOption.TrueAudioNext) ? customSettings.MaxSources : settings.MaxSources
                };

                if (reason != GameEngineStateInitReason.ExportingScene)
                    scene.Create(computeDevice, simulationSettings, context);

                if (reason == GameEngineStateInitReason.Playing)
                    probeManager.Create(context);

                if (reason != GameEngineStateInitReason.ExportingScene &&
                    reason != GameEngineStateInitReason.GeneratingProbes)
                {
                    try
                    {
                        environment.Create(computeDevice, simulationSettings, scene, probeManager, context);
                    }
                    catch (Exception e)
                    {
                        throw e;
                    }
                }
            }
        }

        public void Destroy()
        {
            environment.Destroy();
            probeManager.Destroy();
            scene.Destroy();
            computeDevice.Destroy();

            PhononCore.iplDestroyContext(ref context);
        }

        public IntPtr Context()
        {
            return context;
        }

        public ComputeDevice ComputeDevice()
        {
            return computeDevice;
        }

        public SimulationSettings SimulationSettings()
        {
            return simulationSettings;
        }

        public Scene Scene()
        {
            return scene;
        }

        public ProbeManager ProbeManager()
        {
            return probeManager;
        }

        public Environment Environment()
        {
            return environment;
        }

        public ConvolutionOption ConvolutionType()
        {
            return convolutionType;
        }

        public void ExportScene(MaterialValue defaultMaterial, bool exportOBJ)
        {
            try
            {
                scene.Export(computeDevice, simulationSettings, defaultMaterial, context, exportOBJ);
            }
            catch (Exception e)
            {
                Debug.LogError("Phonon Geometry not attached. " + e.Message);
            }
        }

        static void LogMessage(string message)
        {
            Debug.Log(message);
        }

        IntPtr              context;
        ComputeDevice       computeDevice       = new ComputeDevice();
        SimulationSettings  simulationSettings;
        Scene               scene               = new Scene();
        ProbeManager        probeManager        = new ProbeManager();
        Environment         environment         = new Environment();
        ConvolutionOption   convolutionType     = ConvolutionOption.Phonon;
    }
}