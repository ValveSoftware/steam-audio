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
        Baking,
        Playing
    }

    public class GameEngineState
    {
        public void Initialize(SimulationSettingsValue settings, ComponentCache componentCache,
            GameEngineStateInitReason reason)
        {
            context = new GlobalContext
            {
                logCallback         = LogMessage,
                allocateCallback    = IntPtr.Zero,
                freeCallback        = IntPtr.Zero
            };

            var customSettings = componentCache.SteamAudioCustomSettings();

            var useOpenCL = false;
            var computeDeviceType = ComputeDeviceType.Any;
            var numComputeUnits = 0;

            if (customSettings)
            {
                if (customSettings.convolutionOption == ConvolutionOption.TrueAudioNext)
                {
                    useOpenCL = true;
                    computeDeviceType = ComputeDeviceType.GPU;
                    numComputeUnits = customSettings.numComputeUnits;
                }
                else if (customSettings.rayTracerOption == SceneType.RadeonRays)
                {
                    useOpenCL = true;
                }
            }

            try
            {
                computeDevice.Create(context, useOpenCL, computeDeviceType, numComputeUnits);
            }
            catch (Exception e)
            {
                throw e;
            }

            var inEditor = !SteamAudioManager.IsAudioEngineInitializing();

            simulationSettings = new SimulationSettings
            {
                sceneType               = (customSettings) ? customSettings.rayTracerOption : SceneType.Phonon,
                rays                    = (inEditor) ? settings.BakeRays : settings.RealtimeRays,
                secondaryRays           = (inEditor) ? settings.BakeSecondaryRays : settings.RealtimeSecondaryRays,
                bounces                 = (inEditor) ? settings.BakeBounces : settings.RealtimeBounces,
                irDuration              = settings.Duration,
                ambisonicsOrder         = settings.AmbisonicsOrder,
                maxConvolutionSources   = settings.MaxSources
            };

            if (reason != GameEngineStateInitReason.ExportingScene)
                scene.Create(computeDevice, simulationSettings, context);

            if (reason == GameEngineStateInitReason.Playing)
                probeManager.Create();

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

        public void Destroy()
        {
            environment.Destroy();
            probeManager.Destroy();
            scene.Destroy();
            computeDevice.Destroy();
        }

        public GlobalContext Context()
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

        GlobalContext       context;
        ComputeDevice       computeDevice       = new ComputeDevice();
        SimulationSettings  simulationSettings;
        Scene               scene               = new Scene();
        ProbeManager        probeManager        = new ProbeManager();
        Environment         environment         = new Environment();
    }
}