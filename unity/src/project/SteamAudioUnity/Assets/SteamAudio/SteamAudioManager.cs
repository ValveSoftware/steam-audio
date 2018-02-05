//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System.Collections;
using UnityEngine;

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Manager")]
    public class SteamAudioManager : MonoBehaviour
    {
        private void Awake()
        {
            if (managerSingleton == null)
                managerSingleton = this;

            Initialize(GameEngineStateInitReason.Playing);
        }

        void OnEnable()
        {
            StartCoroutine(EndOfFrameUpdate());
        }

        private void OnDestroy()
        {
            Destroy();
            ClearSingleton();
        }

        void OnApplicationQuit()
        {
            PhononCore.iplCleanup();
        }

        IEnumerator EndOfFrameUpdate()
        {
            while (true)
            {
                var audioListener = currentAudioListener;
                if (updateComponents)
                    audioListener = managerData.componentCache.AudioListener();

                if (audioListener != null)
                {
                    var listenerPosition = Common.ConvertVector(audioListener.transform.position);
                    var listenerAhead = Common.ConvertVector(audioListener.transform.forward);
                    var listenerUp = Common.ConvertVector(audioListener.transform.up);

                    managerData.audioEngineState.UpdateListener(listenerPosition, listenerAhead, listenerUp);
                }

                yield return waitForEndOfFrame;
            }
        }

        // Initializes Phonon Manager, in particular various Phonon API handles contained within Phonon Manager.
        // Initialize will be performed only once despite repeated calls to Initialize without calls to Destroy.
        public void Initialize(GameEngineStateInitReason reason)
        {
            managerData.Initialize(reason, audioEngine, simulationValue);
        }

        // Destroys Phonon Manager.
        public void Destroy()
        {
            managerData.Destroy();
        }

        public static bool IsAudioEngineInitializing()
        {
#if UNITY_EDITOR
            return Application.isEditor && UnityEditor.EditorApplication.isPlayingOrWillChangePlaymode;
#else
            return true;
#endif
        }

        // Exports Unity Scene and saves it in a phononscene file.
        public void ExportScene(bool exportOBJ)
        {
            Initialize(GameEngineStateInitReason.ExportingScene);
            managerData.gameEngineState.ExportScene(materialValue, exportOBJ);
            Destroy();
        }

        public AudioListener AudioListener()
        {
            return managerData.componentCache.AudioListener();
        }

        public SteamAudioListener SteamAudioListener()
        {
            return managerData.componentCache.SteamAudioListener();
        }

        public GameEngineState GameEngineState()
        {
            return managerData.gameEngineState;
        }

        public AudioEngineState AudioEngineState()
        {
            return managerData.audioEngineState;
        }

        public ManagerData ManagerData()
        {
            return managerData;
        }

        public static SteamAudioManager GetSingleton()
        {
            if (managerSingleton == null)
                managerSingleton = FindObjectOfType<SteamAudioManager>();

            return managerSingleton;
        }

        static void ClearSingleton()
        {
            managerSingleton = null;
        }

        static SteamAudioManager managerSingleton = null;

        ManagerData         managerData         = new ManagerData();
        WaitForEndOfFrame   waitForEndOfFrame   = new WaitForEndOfFrame();

        public AudioEngine              audioEngine             = AudioEngine.UnityNative;
        public MaterialPreset           materialPreset          = MaterialPreset.Generic;
        public MaterialValue            materialValue;
        public SimulationSettingsPreset simulationPreset        = SimulationSettingsPreset.Low;
        public SimulationSettingsValue  simulationValue;
        public bool                     updateComponents        = true;
        public AudioListener            currentAudioListener    = null;
        public bool                     showLoadTimeOptions     = false;
        public bool                     showMassBakingOptions   = false;
        public Baker                    phononBaker             = new Baker();
    }
}