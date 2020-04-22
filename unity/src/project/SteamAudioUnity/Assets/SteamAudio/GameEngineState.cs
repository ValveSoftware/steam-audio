//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Collections.Generic;
using System.IO;
using AOT;
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
                var maxCUsToReserve = 0;
                var fractionCUsForIRUpdate = .0f;

                convolutionType = ConvolutionOption.Phonon;
                var rayTracer = SceneType.Phonon;

                // TAN is enabled for realtime.
                if (customSettings && customSettings.ConvolutionType() == ConvolutionOption.TrueAudioNext 
                    && reason == GameEngineStateInitReason.Playing)
                {
                    convolutionType = customSettings.ConvolutionType();

                    useOpenCL = true;
                    computeDeviceType = ComputeDeviceType.GPU;
                    maxCUsToReserve = customSettings.maxComputeUnitsToReserve;
                    fractionCUsForIRUpdate = customSettings.fractionComputeUnitsForIRUpdate;
                }

                // Enable some settings which are commong whether Radeon Rays is enabled for baking or realtime.
                if (customSettings && (reason == GameEngineStateInitReason.Baking || reason == GameEngineStateInitReason.Playing))
                {
                    if (customSettings.RayTracerType() != SceneType.RadeonRays)
                    {
                        rayTracer = customSettings.RayTracerType();
                    }
                    else
                    {
                        useOpenCL = true;
                        rayTracer = SceneType.RadeonRays;
                        computeDeviceType = ComputeDeviceType.GPU;
                    }
                }

                // Enable additional settings when Radeon Rays is enabled for realtime but TAN is not.
                if (customSettings && customSettings.RayTracerType() == SceneType.RadeonRays 
                    && customSettings.ConvolutionType() != ConvolutionOption.TrueAudioNext 
                    && reason == GameEngineStateInitReason.Playing)
                {
                    maxCUsToReserve = customSettings.maxComputeUnitsToReserve;
                    fractionCUsForIRUpdate = 1.0f;
                }

                try
                {
                    var deviceFilter = new ComputeDeviceFilter
                    {
                        type = computeDeviceType,
                        maxCUsToReserve = maxCUsToReserve,
                        fractionCUsForIRUpdate = fractionCUsForIRUpdate
                    };

                    computeDevice.Create(context, useOpenCL, deviceFilter);
                }
                catch (Exception e)
                {
                    if (customSettings && convolutionType == ConvolutionOption.TrueAudioNext) 
                    {
                        var eInEditor = !SteamAudioManager.IsAudioEngineInitializing();
                        if (eInEditor && (!File.Exists(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64/tanrt64.dll"))) 
                        {
                            throw new Exception(
                                "Steam Audio configured to use TrueAudio Next, but TrueAudio Next support package " +
                                "not installed. Please import SteamAudio_TrueAudioNext.unitypackage in order to use " +
                                "TrueAudio Next support for Steam Audio.");
                        }
                        else
                        {
                            Debug.LogWarning(String.Format("Unable to create compute device: {0}. Using Phonon convolution and raytracer.",
                                e.Message));
                        }
                    }
                    else 
                    {
                        Debug.LogWarning(String.Format("Unable to create compute device: {0}. Using Phonon convolution and raytracer.",
                            e.Message));
                    }

                    convolutionType = ConvolutionOption.Phonon;
                    rayTracer = SceneType.Phonon;
                }

                var inEditor = !SteamAudioManager.IsAudioEngineInitializing();

                var maxSources = settings.MaxSources;
                if (customSettings && convolutionType == ConvolutionOption.TrueAudioNext) {
                    maxSources = customSettings.MaxSources;
                }
                if (rayTracer == SceneType.RadeonRays && reason == GameEngineStateInitReason.Baking) {
                    maxSources = customSettings.BakingBatchSize;
                }

                simulationSettings = new SimulationSettings
                {
                    sceneType               = rayTracer,
                    maxOcclusionSamples     = settings.MaxOcclusionSamples,
                    rays                    = (inEditor) ? settings.BakeRays : settings.RealtimeRays,
                    secondaryRays           = (inEditor) ? settings.BakeSecondaryRays : settings.RealtimeSecondaryRays,
                    bounces                 = (inEditor) ? settings.BakeBounces : settings.RealtimeBounces,
                    threads                 = (inEditor) ? (int) Mathf.Max(1, (settings.BakeThreadsPercentage * SystemInfo.processorCount) / 100.0f) : (int) Mathf.Max(1, (settings.RealtimeThreadsPercentage * SystemInfo.processorCount) / 100.0f),
                    irDuration              = (customSettings && convolutionType == ConvolutionOption.TrueAudioNext) ? customSettings.Duration : settings.Duration,
                    ambisonicsOrder         = (customSettings && convolutionType == ConvolutionOption.TrueAudioNext) ? customSettings.AmbisonicsOrder : settings.AmbisonicsOrder,
                    maxConvolutionSources   = maxSources,
                    bakingBatchSize         = (rayTracer == SceneType.RadeonRays) ? customSettings.BakingBatchSize : 1,
                    irradianceMinDistance   = settings.IrradianceMinDistance
                };

#if UNITY_EDITOR
                if (customSettings) {
                    if (rayTracer == SceneType.Embree) {
                        if (!File.Exists(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64/embree.dll")) {
                            throw new Exception(
                                "Steam Audio configured to use Embree, but Embree support package not installed. " +
                                "Please import SteamAudio_Embree.unitypackage in order to use Embree support for " +
                                "Steam Audio.");
                        }
                    } else if (rayTracer == SceneType.RadeonRays) {
                        if (!File.Exists(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64/RadeonRays.dll")) {
                            throw new Exception(
                                "Steam Audio configured to use Radeon Rays, but Radeon Rays support package not " +
                                "installed. Please import SteamAudio_RadeonRays.unitypackage in order to use Radeon " +
                                "Rays support for Steam Audio.");
                        }
                    }

                    if (convolutionType == ConvolutionOption.TrueAudioNext) {
                        if (!File.Exists(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64/tanrt64.dll")) {
                            throw new Exception(
                                "Editor: Steam Audio configured to use TrueAudio Next, but TrueAudio Next support package " +
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
                scene.Export(computeDevice, defaultMaterial, context, exportOBJ);
            }
            catch (Exception e)
            {
                Debug.LogError(e.Message);
            }
        }

        [MonoPInvokeCallback(typeof(LogCallback))]
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

        public Dictionary<string, IntPtr> instancedScenes = null;
    }
}