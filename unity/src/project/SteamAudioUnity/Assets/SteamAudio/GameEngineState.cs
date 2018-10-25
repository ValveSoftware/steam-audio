//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.IO;
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
                    convolutionType = customSettings.ConvolutionType();

                    if (convolutionType == ConvolutionOption.TrueAudioNext)
                    {
                        useOpenCL = true;
                        requiresTan = true;
                        computeDeviceType = ComputeDeviceType.GPU;
                        minReservableCUs = customSettings.minComputeUnitsToReserve;
                        maxCUsToReserve = customSettings.maxComputeUnitsToReserve;
                    }
                    else if (customSettings.RayTracerType() == SceneType.RadeonRays)
                    {
                        useOpenCL = true;
                    }
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
                }

                var inEditor = !SteamAudioManager.IsAudioEngineInitializing();

                var maxSources = settings.MaxSources;
                if (customSettings && convolutionType == ConvolutionOption.TrueAudioNext) {
                    maxSources = customSettings.MaxSources;
                }
                if (customSettings && customSettings.RayTracerType() == SceneType.RadeonRays && reason == GameEngineStateInitReason.Baking) {
                    maxSources = customSettings.BakingBatchSize;
                }

                var rayTracer = SceneType.Phonon;
                if (customSettings) {
                    if (customSettings.RayTracerType() != SceneType.RadeonRays || 
                        reason != GameEngineStateInitReason.Playing) {
                        rayTracer = customSettings.RayTracerType();
                    }
                }

                simulationSettings = new SimulationSettings
                {
                    sceneType               = rayTracer,
                    occlusionSamples        = settings.OcclusionSamples,
                    rays                    = (inEditor) ? settings.BakeRays : settings.RealtimeRays,
                    secondaryRays           = (inEditor) ? settings.BakeSecondaryRays : settings.RealtimeSecondaryRays,
                    bounces                 = (inEditor) ? settings.BakeBounces : settings.RealtimeBounces,
                    threads                 = (inEditor) ? (int) Mathf.Max(1, (settings.BakeThreadsPercentage * SystemInfo.processorCount) / 100.0f) : (int) Mathf.Max(1, (settings.RealtimeThreadsPercentage * SystemInfo.processorCount) / 100.0f),
                    irDuration              = (customSettings && convolutionType == ConvolutionOption.TrueAudioNext) ? customSettings.Duration : settings.Duration,
                    ambisonicsOrder         = (customSettings && convolutionType == ConvolutionOption.TrueAudioNext) ? customSettings.AmbisonicsOrder : settings.AmbisonicsOrder,
                    maxConvolutionSources   = maxSources,
                    bakingBatchSize         = (customSettings && customSettings.RayTracerType() == SceneType.RadeonRays) ? customSettings.BakingBatchSize : 1
                };

#if UNITY_EDITOR
                if (customSettings) {
                    if (customSettings.RayTracerType() == SceneType.Embree) {
                        if (!File.Exists(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64/embree.dll")) {
                            throw new Exception(
                                "Steam Audio configured to use Embree, but Embree support package not installed. " +
                                "Please import SteamAudio_Embree.unitypackage in order to use Embree support for " +
                                "Steam Audio.");
                        }
                    } else if (customSettings.RayTracerType() == SceneType.RadeonRays) {
                        if (!File.Exists(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64/RadeonRays.dll")) {
                            throw new Exception(
                                "Steam Audio configured to use Radeon Rays, but Radeon Rays support package not " +
                                "installed. Please import SteamAudio_RadeonRays.unitypackage in order to use Radeon " +
                                "Rays support for Steam Audio.");
                        }
                    }

                    if (customSettings.ConvolutionType() == ConvolutionOption.TrueAudioNext) {
                        if (!File.Exists(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64/tanrt64.dll")) {
                            throw new Exception(
                                "Steam Audio configured to use TrueAudio Next, but TrueAudio Next support package " +
                                "not installed. Please import SteamAudio_TrueAudioNext.unitypackage in order to use " +
                                "TrueAudio Next support for Steam Audio.");
                        }
                    }
                }
#endif

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