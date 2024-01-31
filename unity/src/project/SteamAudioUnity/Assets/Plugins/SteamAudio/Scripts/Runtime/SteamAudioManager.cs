//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using AOT;
using UnityEngine;
using UnityEngine.SceneManagement;
#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.SceneManagement;
using System.IO;
#if UNITY_2019_2_OR_NEWER
using UnityEditor.PackageManager;
#endif
#endif

namespace SteamAudio
{
    public enum ManagerInitReason
    {
        ExportingScene,
        GeneratingProbes,
        EditingProbes,
        Baking,
        Playing
    }

    public class SteamAudioManager : MonoBehaviour
    {
        [Header("HRTF Settings")]
        public int currentHRTF = 0;

#if STEAMAUDIO_ENABLED
        public string[] hrtfNames = null;

        int mNumCPUCores = 0;
        AudioSettings mAudioSettings;
        Context mContext = null;
        HRTF[] mHRTFs = null;
        EmbreeDevice mEmbreeDevice = null;
        bool mEmbreeInitFailed = false;
        OpenCLDevice mOpenCLDevice = null;
        bool mOpenCLInitFailed = false;
        RadeonRaysDevice mRadeonRaysDevice = null;
        bool mRadeonRaysInitFailed = false;
        TrueAudioNextDevice mTrueAudioNextDevice = null;
        bool mTrueAudioNextInitFailed = false;
        Scene mCurrentScene = null;
        Dictionary<string, int> mDynamicObjectRefCounts = new Dictionary<string, int>();
        Dictionary<string, Scene> mDynamicObjects = new Dictionary<string, Scene>();
        Simulator mSimulator = null;
        AudioEngineState mAudioEngineState = null;
        Transform mListener = null;
        SteamAudioListener mListenerComponent = null;
        HashSet<SteamAudioSource> mSources = new HashSet<SteamAudioSource>();
        HashSet<SteamAudioListener> mListeners = new HashSet<SteamAudioListener>();
        RaycastHit[] mRayHits = new RaycastHit[1];
        IntPtr mMaterialBuffer = IntPtr.Zero;
        Thread mSimulationThread = null;
        EventWaitHandle mSimulationThreadWaitHandle = null;
        bool mStopSimulationThread = false;
        bool mSimulationCompleted = false;
        float mSimulationUpdateTimeElapsed = 0.0f;
        bool mSceneCommitRequired = false;
        Camera mMainCamera;

        static SteamAudioManager sSingleton = null;

        public static SteamAudioManager Singleton
        {
            get
            {
                return sSingleton;
            }
        }

        public static Context Context
        {
            get
            {
                return sSingleton.mContext;
            }
        }

        public static HRTF CurrentHRTF
        {
            get
            {
                return sSingleton.mHRTFs[sSingleton.currentHRTF];
            }
        }

        public static IntPtr EmbreeDevice
        {
            get
            {
                return sSingleton.mEmbreeDevice.Get();
            }
        }

        public static IntPtr OpenCLDevice
        {
            get
            {
                return sSingleton.mOpenCLDevice.Get();
            }
        }

        public static IntPtr RadeonRaysDevice
        {
            get
            {
                return sSingleton.mRadeonRaysDevice.Get();
            }
        }

        public static IntPtr TrueAudioNextDevice
        {
            get
            {
                return sSingleton.mTrueAudioNextDevice.Get();
            }
        }

        public static Scene CurrentScene
        {
            get
            {
                return sSingleton.mCurrentScene;
            }
        }

        public static Simulator Simulator
        {
            get
            {
                return sSingleton.mSimulator;
            }
        }

        public static AudioSettings AudioSettings
        {
            get
            {
                return sSingleton.mAudioSettings;
            }
        }

        public static AudioEngineState GetAudioEngineState()
        {
            return sSingleton.mAudioEngineState;
        }

        public static SteamAudioListener GetSteamAudioListener()
        {
            if (sSingleton.mListenerComponent == null)
                return null;

            return sSingleton.mListenerComponent;
        }

        public int NumThreadsForCPUCorePercentage(int percentage)
        {
            return (int)Mathf.Max(1, (percentage * mNumCPUCores) / 100.0f);
        }

        public static SceneType GetSceneType()
        {
            var sceneType = SteamAudioSettings.Singleton.sceneType;

            if ((sceneType == SceneType.Embree && sSingleton.mEmbreeInitFailed) ||
                (sceneType == SceneType.RadeonRays && (sSingleton.mOpenCLInitFailed || sSingleton.mRadeonRaysInitFailed)))
            {
                sceneType = SceneType.Default;
            }

            return sceneType;
        }

        public static ReflectionEffectType GetReflectionEffectType()
        {
            var reflectionEffectType = SteamAudioSettings.Singleton.reflectionEffectType;

            if ((reflectionEffectType == ReflectionEffectType.TrueAudioNext && (sSingleton.mOpenCLInitFailed || sSingleton.mTrueAudioNextInitFailed)))
            {
                reflectionEffectType = ReflectionEffectType.Convolution;
            }

            return reflectionEffectType;
        }

        public static PerspectiveCorrection GetPerspectiveCorrection()
        {
            if (!SteamAudioSettings.Singleton.perspectiveCorrection)
                return default;

            var mainCamera = Singleton.GetMainCamera();
            PerspectiveCorrection correction = default;
            if (mainCamera != null && mainCamera.aspect > .0f)
            {
                correction.enabled = SteamAudioSettings.Singleton.perspectiveCorrection ? Bool.True : Bool.False;
                correction.xfactor = 1.0f * SteamAudioSettings.Singleton.perspectiveCorrectionFactor;
                correction.yfactor = correction.xfactor / mainCamera.aspect;

                // Camera space matches OpenGL convention. No need to transform matrix to ConvertTransform.
                correction.transform = Common.TransformMatrix(mainCamera.projectionMatrix * mainCamera.worldToCameraMatrix);
            }

            return correction;
        }

        public Camera GetMainCamera()
        {
            return mMainCamera;
        }

        public static SimulationSettings GetSimulationSettings(bool baking)
        {
            var simulationSettings = new SimulationSettings { };
            simulationSettings.sceneType = GetSceneType();
            simulationSettings.reflectionType = GetReflectionEffectType();

            if (baking)
            {
                simulationSettings.flags = SimulationFlags.Reflections | SimulationFlags.Pathing;
                simulationSettings.maxNumRays = SteamAudioSettings.Singleton.bakingRays;
                simulationSettings.numDiffuseSamples = 1024;
                simulationSettings.maxDuration = SteamAudioSettings.Singleton.bakingDuration;
                simulationSettings.maxOrder = SteamAudioSettings.Singleton.bakingAmbisonicOrder;
                simulationSettings.numThreads = sSingleton.NumThreadsForCPUCorePercentage(SteamAudioSettings.Singleton.bakingCPUCoresPercentage);
                simulationSettings.rayBatchSize = 16;
            }
            else
            {
                simulationSettings.flags = SimulationFlags.Direct | SimulationFlags.Reflections | SimulationFlags.Pathing;
                simulationSettings.maxNumOcclusionSamples = SteamAudioSettings.Singleton.maxOcclusionSamples;
                simulationSettings.maxNumRays = SteamAudioSettings.Singleton.realTimeRays;
                simulationSettings.numDiffuseSamples = 1024;
                simulationSettings.maxDuration = (simulationSettings.reflectionType == ReflectionEffectType.TrueAudioNext) ? SteamAudioSettings.Singleton.TANDuration : SteamAudioSettings.Singleton.realTimeDuration;
                simulationSettings.maxOrder = (simulationSettings.reflectionType == ReflectionEffectType.TrueAudioNext) ? SteamAudioSettings.Singleton.TANAmbisonicOrder : SteamAudioSettings.Singleton.realTimeAmbisonicOrder;
                simulationSettings.maxNumSources = (simulationSettings.reflectionType == ReflectionEffectType.TrueAudioNext) ? SteamAudioSettings.Singleton.TANMaxSources : SteamAudioSettings.Singleton.realTimeMaxSources;
                simulationSettings.numThreads = sSingleton.NumThreadsForCPUCorePercentage(SteamAudioSettings.Singleton.realTimeCPUCoresPercentage);
                simulationSettings.rayBatchSize = 16;
                simulationSettings.numVisSamples = SteamAudioSettings.Singleton.bakingVisibilitySamples;
                simulationSettings.samplingRate = AudioSettings.samplingRate;
                simulationSettings.frameSize = AudioSettings.frameSize;
            }

            if (simulationSettings.sceneType == SceneType.RadeonRays)
            {
                simulationSettings.openCLDevice = sSingleton.mOpenCLDevice.Get();
                simulationSettings.radeonRaysDevice = sSingleton.mRadeonRaysDevice.Get();
            }

            if (!baking && simulationSettings.reflectionType == ReflectionEffectType.TrueAudioNext)
            {
                simulationSettings.openCLDevice = sSingleton.mOpenCLDevice.Get();
                simulationSettings.tanDevice = sSingleton.mTrueAudioNextDevice.Get();
            }

            return simulationSettings;
        }

        // This method is called at app startup (see above).
        void OnApplicationStart(ManagerInitReason reason)
        {
            if (reason == ManagerInitReason.Playing)
            {
                SceneManager.sceneLoaded += OnSceneLoaded;
                SceneManager.sceneUnloaded += OnSceneUnloaded;
            }

            mNumCPUCores = SystemInfo.processorCount;

            mContext = new Context();

            if (reason == ManagerInitReason.Playing)
            {
                mAudioSettings = AudioEngineStateHelpers.Create(SteamAudioSettings.Singleton.audioEngine).GetAudioSettings();

                mHRTFs = new HRTF[SteamAudioSettings.Singleton.SOFAFiles.Length + 1];

                hrtfNames = new string[SteamAudioSettings.Singleton.SOFAFiles.Length + 1];
                hrtfNames[0] = "Default";
                for (var i = 0; i < SteamAudioSettings.Singleton.SOFAFiles.Length; ++i)
                {
                    if (SteamAudioSettings.Singleton.SOFAFiles[i])
                        hrtfNames[i + 1] = SteamAudioSettings.Singleton.SOFAFiles[i].sofaName;
                    else
                        hrtfNames[i + 1] = null;
                }

                mHRTFs[0] = new HRTF(mContext, mAudioSettings, null, null, SteamAudioSettings.Singleton.hrtfVolumeGainDB, SteamAudioSettings.Singleton.hrtfNormalizationType);

                for (var i = 0; i < SteamAudioSettings.Singleton.SOFAFiles.Length; ++i)
                {
                    if (SteamAudioSettings.Singleton.SOFAFiles[i])
                    {
                        mHRTFs[i + 1] = new HRTF(mContext, mAudioSettings,
                            SteamAudioSettings.Singleton.SOFAFiles[i].sofaName,
                            SteamAudioSettings.Singleton.SOFAFiles[i].data,
                            SteamAudioSettings.Singleton.SOFAFiles[i].volume,
                            SteamAudioSettings.Singleton.SOFAFiles[i].normType);
                    }
                    else
                    {
                        Debug.LogWarning("SOFA Asset File Missing. Assigning default HRTF.");
                        mHRTFs[i + 1] = mHRTFs[0];
                    }
                }
            }

            if (reason != ManagerInitReason.EditingProbes)
            {
                if (SteamAudioSettings.Singleton.sceneType == SceneType.Embree)
                {
                    try
                    {
                        mEmbreeInitFailed = false;

                        mEmbreeDevice = new EmbreeDevice(mContext);
                    }
                    catch (Exception e)
                    {
                        mEmbreeInitFailed = true;

                        Debug.LogException(e);
                        Debug.LogWarning("Embree initialization failed, reverting to Phonon for ray tracing.");
                    }
                }

                var requiresTAN = (SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext);

                if (SteamAudioSettings.Singleton.sceneType == SceneType.RadeonRays ||
                    SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext)
                {
                    try
                    {
                        mOpenCLInitFailed = false;

                        mOpenCLDevice = new OpenCLDevice(mContext, SteamAudioSettings.Singleton.deviceType,
                            SteamAudioSettings.Singleton.maxReservedComputeUnits,
                            SteamAudioSettings.Singleton.fractionComputeUnitsForIRUpdate,
                            requiresTAN);
                    }
                    catch (Exception e)
                    {
                        mOpenCLInitFailed = true;

                        Debug.LogException(e);

                        var warningMessage = "OpenCL initialization failed.";
                        if (SteamAudioSettings.Singleton.sceneType == SceneType.RadeonRays)
                            warningMessage += " Reverting to Phonon for ray tracing.";
                        if (SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext)
                            warningMessage += " Reverting to Convolution for reflection effect processing.";

                        Debug.LogWarning(warningMessage);
                    }
                }

                if (SteamAudioSettings.Singleton.sceneType == SceneType.RadeonRays &&
                    !mOpenCLInitFailed)
                {
                    try
                    {
                        mRadeonRaysInitFailed = false;

                        mRadeonRaysDevice = new RadeonRaysDevice(mOpenCLDevice);
                    }
                    catch (Exception e)
                    {
                        mRadeonRaysInitFailed = true;

                        Debug.LogException(e);
                        Debug.LogWarning("Radeon Rays initialization failed, reverting to Phonon for ray tracing.");
                    }
                }

                if (SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext &&
                    reason == ManagerInitReason.Playing &&
                    !mOpenCLInitFailed)
                {
                    try
                    {
                        mTrueAudioNextInitFailed = false;

                        var frameSize = AudioSettings.frameSize;
                        var irSize = Mathf.CeilToInt(SteamAudioSettings.Singleton.realTimeDuration * AudioSettings.samplingRate);
                        var order = SteamAudioSettings.Singleton.realTimeAmbisonicOrder;
                        var maxSources = SteamAudioSettings.Singleton.TANMaxSources;

                        mTrueAudioNextDevice = new TrueAudioNextDevice(mOpenCLDevice, frameSize, irSize,
                            order, maxSources);
                    }
                    catch (Exception e)
                    {
                        mTrueAudioNextInitFailed = true;

                        Debug.LogException(e);
                        Debug.LogWarning("TrueAudio Next initialization failed, reverting to Convolution for reflection effect processing.");
                    }
                }
            }

            if (reason == ManagerInitReason.Playing)
            {
                var simulationSettings = GetSimulationSettings(false);
                var perspectiveCorrection = GetPerspectiveCorrection();

                mSimulator = new Simulator(mContext, simulationSettings);

                mSimulationThreadWaitHandle = new EventWaitHandle(false, EventResetMode.AutoReset);

                mSimulationThread = new Thread(RunSimulation);
                mSimulationThread.Start();

                mAudioEngineState = AudioEngineState.Create(SteamAudioSettings.Singleton.audioEngine);
                if (mAudioEngineState != null)
                {
                    mAudioEngineState.Initialize(mContext.Get(), mHRTFs[0].Get(), simulationSettings, perspectiveCorrection);
                }

#if UNITY_EDITOR && UNITY_2019_3_OR_NEWER
                // If the developer has disabled scene reload, SceneManager.sceneLoaded won't fire during initial load
                if (EditorSettings.enterPlayModeOptions.HasFlag(EnterPlayModeOptions.DisableSceneReload))
                {
                    OnSceneLoaded(SceneManager.GetActiveScene(), LoadSceneMode.Single);
                }
#endif
            }
        }

        // This method is called at app shutdown.
        void OnApplicationQuit()
        {
            ShutDown();
        }

        // This method is called when a scene is loaded.
        void OnSceneLoaded(UnityEngine.SceneManagement.Scene scene, LoadSceneMode loadSceneMode)
        {
            LoadScene(scene, mContext, additive: (loadSceneMode == LoadSceneMode.Additive));

            NotifyMainCameraChanged();
            NotifyAudioListenerChanged();
        }

        // This method is called when a scene is unloaded.
        void OnSceneUnloaded(UnityEngine.SceneManagement.Scene scene)
        {
            RemoveAllDynamicObjects();
        }

        // Call this function when you create a new AudioListener component (or its equivalent, if you are using
        // third-party audio middleware).
        public static void NotifyAudioListenerChanged()
        {
            sSingleton.mListener = AudioEngineStateHelpers.Create(SteamAudioSettings.Singleton.audioEngine).GetListenerTransform();
            if (sSingleton.mListener)
            {
                sSingleton.mListenerComponent = sSingleton.mListener.GetComponent<SteamAudioListener>();
            }
        }

        // Call this function when you create or change the main camera.
        public static void NotifyMainCameraChanged()
        {
            sSingleton.mMainCamera = Camera.main;
        }

        // Call this function to request that changes to a scene be committed. Call only when changes have happened.
        public static void ScheduleCommitScene()
        {
            sSingleton.mSceneCommitRequired = true;
        }

#if STEAMAUDIO_ENABLED
        private void LateUpdate()
        {
            if (mAudioEngineState == null)
                return;

            var perspectiveCorrection = GetPerspectiveCorrection();
            mAudioEngineState.SetPerspectiveCorrection(perspectiveCorrection);

            mAudioEngineState.SetHRTF(CurrentHRTF.Get());

            if (mCurrentScene == null || mSimulator == null)
                return;

            if (mSimulationThread.ThreadState == ThreadState.WaitSleepJoin)
            {
                if (mSceneCommitRequired)
                {
                    mCurrentScene.Commit();
                    mSceneCommitRequired = false;
                }

                mSimulator.SetScene(mCurrentScene);
                mSimulator.Commit();
            }

            var sharedInputs = new SimulationSharedInputs { };

            if (mListener != null)
            {
                sharedInputs.listener.origin = Common.ConvertVector(mListener.position);
                sharedInputs.listener.ahead = Common.ConvertVector(mListener.forward);
                sharedInputs.listener.up = Common.ConvertVector(mListener.up);
                sharedInputs.listener.right = Common.ConvertVector(mListener.right);
            }

            sharedInputs.numRays = SteamAudioSettings.Singleton.realTimeRays;
            sharedInputs.numBounces = SteamAudioSettings.Singleton.realTimeBounces;
            sharedInputs.duration = SteamAudioSettings.Singleton.realTimeDuration;
            sharedInputs.order = SteamAudioSettings.Singleton.realTimeAmbisonicOrder;
            sharedInputs.irradianceMinDistance = SteamAudioSettings.Singleton.realTimeIrradianceMinDistance;
            sharedInputs.pathingVisualizationCallback = null;
            sharedInputs.pathingUserData = IntPtr.Zero;

            mSimulator.SetSharedInputs(SimulationFlags.Direct, sharedInputs);

            foreach (var source in mSources)
            {
                source.SetInputs(SimulationFlags.Direct);
            }

            foreach (var listener in mListeners)
            {
                listener.SetInputs(SimulationFlags.Direct);
            }

            mSimulator.RunDirect();

            foreach (var source in mSources)
            {
                source.UpdateOutputs(SimulationFlags.Direct);
            }

            foreach (var listener in mListeners)
            {
                listener.UpdateOutputs(SimulationFlags.Direct);
            }

            mSimulationUpdateTimeElapsed += Time.deltaTime;
            if (mSimulationUpdateTimeElapsed < SteamAudioSettings.Singleton.simulationUpdateInterval)
                return;

            mSimulationUpdateTimeElapsed = 0.0f;

            if (mSimulationThread.ThreadState == ThreadState.WaitSleepJoin)
            {
                if (mSimulationCompleted)
                {
                    mSimulationCompleted = false;

                    foreach (var source in mSources)
                    {
                        source.UpdateOutputs(SimulationFlags.Reflections | SimulationFlags.Pathing);
                    }

                    foreach (var listener in mListeners)
                    {
                        listener.UpdateOutputs(SimulationFlags.Reflections | SimulationFlags.Pathing);
                    }
                }

                mSimulator.SetSharedInputs(SimulationFlags.Reflections | SimulationFlags.Pathing, sharedInputs);

                foreach (var source in mSources)
                {
                    source.SetInputs(SimulationFlags.Reflections | SimulationFlags.Pathing);
                }

                foreach (var listener in mListeners)
                {
                    listener.SetInputs(SimulationFlags.Reflections | SimulationFlags.Pathing);
                }

                if (SteamAudioSettings.Singleton.sceneType == SceneType.Custom)
                {
                    // The Unity ray tracer must be called from the main thread only, so run the simulation here.
                    // It's not suitable for heavy workloads anyway, so we assume that the performance hit is
                    // acceptable. If not, we recommend switching to one of the other ray tracers.
                    RunSimulationInternal();
                }
                else
                {
                    mSimulationThreadWaitHandle.Set();
                }
            }
        }
#endif

        void RunSimulationInternal()
        {
            if (mSimulator == null)
                return;

            mSimulator.RunReflections();
            mSimulator.RunPathing();

            mSimulationCompleted = true;
        }

        void RunSimulation()
        {
            while (!mStopSimulationThread)
            {
                mSimulationThreadWaitHandle.WaitOne();

                if (mStopSimulationThread)
                    break;

                RunSimulationInternal();
            }
        }

        public static void Initialize(ManagerInitReason reason)
        {
            var managerObject = new GameObject("Steam Audio Manager");
            var manager = managerObject.AddComponent<SteamAudioManager>();

            if (reason == ManagerInitReason.Playing)
            {
                DontDestroyOnLoad(managerObject);
            }

            sSingleton = manager;

            manager.OnApplicationStart(reason);
        }

        public static void ShutDown()
        {
            if (sSingleton.mSimulationThread != null)
            {
                sSingleton.mStopSimulationThread = true;
                sSingleton.mSimulationThreadWaitHandle.Set();
                sSingleton.mSimulationThread.Join();
            }

            RemoveAllDynamicObjects(force: true);
            RemoveAllAdditiveScenes();

            if (sSingleton.mAudioEngineState != null)
            {
                sSingleton.mAudioEngineState.Destroy();
            }

            if (sSingleton.mSimulator != null)
            {
                sSingleton.mSimulator.Release();
                sSingleton.mSimulator = null;
            }

            if (sSingleton.mTrueAudioNextDevice != null)
            {
                sSingleton.mTrueAudioNextDevice.Release();
                sSingleton.mTrueAudioNextDevice = null;
            }

            if (sSingleton.mRadeonRaysDevice != null)
            {
                sSingleton.mRadeonRaysDevice.Release();
                sSingleton.mRadeonRaysDevice = null;
            }

            if (sSingleton.mOpenCLDevice != null)
            {
                sSingleton.mOpenCLDevice.Release();
                sSingleton.mOpenCLDevice = null;
            }

            if (sSingleton.mEmbreeDevice != null)
            {
                sSingleton.mEmbreeDevice.Release();
                sSingleton.mEmbreeDevice = null;
            }

            if (sSingleton.mHRTFs != null)
            {
                for (var i = 0; i < sSingleton.mHRTFs.Length; ++i)
                {
                    sSingleton.mHRTFs[i].Release();
                    sSingleton.mHRTFs[i] = null;
                }
            }

            SceneManager.sceneLoaded -= sSingleton.OnSceneLoaded;
            SceneManager.sceneUnloaded -= sSingleton.OnSceneUnloaded;

            sSingleton.mContext.Release();
            sSingleton.mContext = null;
        }

        public static void Reinitialize()
        {
            if (sSingleton.mSimulationThread != null)
            {
                sSingleton.mStopSimulationThread = true;
                sSingleton.mSimulationThreadWaitHandle.Set();
                sSingleton.mSimulationThread.Join();
            }

            RemoveAllDynamicObjects(force: true);
            RemoveAllAdditiveScenes();

            if (sSingleton.mAudioEngineState != null)
            {
                sSingleton.mAudioEngineState.Destroy();
            }

            sSingleton.mSimulator = null;

            UnityEngine.AudioSettings.Reset(UnityEngine.AudioSettings.GetConfiguration());

            if ((sSingleton.mEmbreeDevice == null || sSingleton.mEmbreeDevice.Get() == IntPtr.Zero)
                && SteamAudioSettings.Singleton.sceneType == SceneType.Embree)
            {
                try
                {
                    sSingleton.mEmbreeInitFailed = false;

                    sSingleton.mEmbreeDevice = new EmbreeDevice(sSingleton.mContext);
                }
                catch (Exception e)
                {
                    sSingleton.mEmbreeInitFailed = true;

                    Debug.LogException(e);
                    Debug.LogWarning("Embree initialization failed, reverting to Phonon for ray tracing.");
                }
            }

            var requiresTAN = (SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext);

            if ((sSingleton.mOpenCLDevice == null || sSingleton.mOpenCLDevice.Get() == IntPtr.Zero) &&
                (SteamAudioSettings.Singleton.sceneType == SceneType.RadeonRays ||
                SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext))
            {
                try
                {
                    sSingleton.mOpenCLInitFailed = false;

                    sSingleton.mOpenCLDevice = new OpenCLDevice(sSingleton.mContext, SteamAudioSettings.Singleton.deviceType,
                        SteamAudioSettings.Singleton.maxReservedComputeUnits,
                        SteamAudioSettings.Singleton.fractionComputeUnitsForIRUpdate,
                        requiresTAN);
                }
                catch (Exception e)
                {
                    sSingleton.mOpenCLInitFailed = true;

                    Debug.LogException(e);

                    var warningMessage = "OpenCL initialization failed.";
                    if (SteamAudioSettings.Singleton.sceneType == SceneType.RadeonRays)
                        warningMessage += " Reverting to Phonon for ray tracing.";
                    if (SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext)
                        warningMessage += " Reverting to Convolution for reflection effect processing.";

                    Debug.LogWarning(warningMessage);
                }
            }

            if ((sSingleton.mRadeonRaysDevice == null || sSingleton.mRadeonRaysDevice.Get() == IntPtr.Zero) &&
                SteamAudioSettings.Singleton.sceneType == SceneType.RadeonRays &&
                !sSingleton.mOpenCLInitFailed)
            {
                try
                {
                    sSingleton.mRadeonRaysInitFailed = false;

                    sSingleton.mRadeonRaysDevice = new RadeonRaysDevice(sSingleton.mOpenCLDevice);
                }
                catch (Exception e)
                {
                    sSingleton.mRadeonRaysInitFailed = true;

                    Debug.LogException(e);
                    Debug.LogWarning("Radeon Rays initialization failed, reverting to Phonon for ray tracing.");
                }
            }

            if ((sSingleton.mTrueAudioNextDevice == null || sSingleton.mTrueAudioNextDevice.Get() == IntPtr.Zero) &&
                SteamAudioSettings.Singleton.reflectionEffectType == ReflectionEffectType.TrueAudioNext &&
                !sSingleton.mOpenCLInitFailed)
            {
                try
                {
                    sSingleton.mTrueAudioNextInitFailed = false;

                    var frameSize = AudioSettings.frameSize;
                    var irSize = Mathf.CeilToInt(SteamAudioSettings.Singleton.realTimeDuration * AudioSettings.samplingRate);
                    var order = SteamAudioSettings.Singleton.realTimeAmbisonicOrder;
                    var maxSources = SteamAudioSettings.Singleton.TANMaxSources;

                    sSingleton.mTrueAudioNextDevice = new TrueAudioNextDevice(sSingleton.mOpenCLDevice, frameSize, irSize,
                        order, maxSources);
                }
                catch (Exception e)
                {
                    sSingleton.mTrueAudioNextInitFailed = true;

                    Debug.LogException(e);
                    Debug.LogWarning("TrueAudio Next initialization failed, reverting to Convolution for reflection effect processing.");
                }
            }

            var simulationSettings = GetSimulationSettings(false);
            var persPectiveCorrection = GetPerspectiveCorrection();

            sSingleton.mSimulator = new Simulator(sSingleton.mContext, simulationSettings);

            sSingleton.mStopSimulationThread = false;
            sSingleton.mSimulationThread = new Thread(sSingleton.RunSimulation);
            sSingleton.mSimulationThread.Start();

            sSingleton.mAudioEngineState = AudioEngineState.Create(SteamAudioSettings.Singleton.audioEngine);
            if (sSingleton.mAudioEngineState != null)
            {
                sSingleton.mAudioEngineState.Initialize(sSingleton.mContext.Get(), sSingleton.mHRTFs[0].Get(), simulationSettings, persPectiveCorrection);

                var listeners = new SteamAudioListener[sSingleton.mListeners.Count];
                sSingleton.mListeners.CopyTo(listeners);
                foreach (var listener in listeners)
                {
                    listener.enabled = false;
                    listener.Reinitialize();
                    listener.enabled = true;
                }
            }
        }

        public static void AddSource(SteamAudioSource source)
        {
            sSingleton.mSources.Add(source);
        }

        public static void RemoveSource(SteamAudioSource source)
        {
            sSingleton.mSources.Remove(source);
        }

        public static void AddListener(SteamAudioListener listener)
        {
            sSingleton.mListeners.Add(listener);
        }

        public static void RemoveListener(SteamAudioListener listener)
        {
            sSingleton.mListeners.Remove(listener);
        }

#if UNITY_EDITOR
        [MenuItem("Steam Audio/Settings", false, 1)]
        public static void EditSettings()
        {
            Selection.activeObject = SteamAudioSettings.Singleton;
#if UNITY_2018_2_OR_NEWER
            EditorApplication.ExecuteMenuItem("Window/General/Inspector");
#else
            EditorApplication.ExecuteMenuItem("Window/Inspector");
#endif
        }

        [MenuItem("Steam Audio/Export Active Scene", false, 12)]
        public static void ExportActiveScene()
        {
            ExportScene(SceneManager.GetActiveScene(), false);
        }

        [MenuItem("Steam Audio/Export All Open Scenes", false, 13)]
        public static void ExportAllOpenScenes()
        {
            for (var i = 0; i < SceneManager.sceneCount; ++i)
            {
                var scene = SceneManager.GetSceneAt(i);

                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Exporting scene: {0}", scene.name), (float)i / (float)SceneManager.sceneCount);

                if (!scene.isLoaded)
                {
                    Debug.LogWarning(string.Format("Scene {0} is not loaded in the hierarchy.", scene.name));
                    continue;
                }

                ExportScene(scene, false);
            }

            EditorUtility.DisplayProgressBar("Steam Audio", "", 1.0f);
            EditorUtility.ClearProgressBar();
        }

        [MenuItem("Steam Audio/Export All Scenes In Build", false, 14)]
        public static void ExportAllScenesInBuild()
        {
            for (var i = 0; i < SceneManager.sceneCountInBuildSettings; ++i)
            {
                var scene = SceneManager.GetSceneByBuildIndex(i);

                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Exporting scene: {0}", scene.name), (float)i / (float)SceneManager.sceneCountInBuildSettings);

                var shouldClose = false;
                if (!scene.isLoaded)
                {
                    scene = EditorSceneManager.OpenScene(SceneUtility.GetScenePathByBuildIndex(i), OpenSceneMode.Additive);
                    shouldClose = true;
                }

                ExportScene(scene, false);

                if (shouldClose)
                {
                    EditorSceneManager.CloseScene(scene, true);
                }
            }

            EditorUtility.DisplayProgressBar("Steam Audio", "", 1.0f);
            EditorUtility.ClearProgressBar();
        }

        [MenuItem("Steam Audio/Export Active Scene To OBJ", false, 25)]
        public static void ExportActiveSceneToOBJ()
        {
            ExportScene(SceneManager.GetActiveScene(), true);
        }

        [MenuItem("Steam Audio/Export Dynamic Objects In Active Scene", false, 36)]
        public static void ExportDynamicObjectsInActiveScene()
        {
            ExportDynamicObjectsInArray(GetDynamicObjectsInScene(SceneManager.GetActiveScene()));
        }

        [MenuItem("Steam Audio/Export Dynamic Objects In All Open Scenes", false, 37)]
        public static void ExportDynamicObjectsInAllOpenScenes()
        {
            for (var i = 0; i < SceneManager.sceneCount; ++i)
            {
                var scene = SceneManager.GetSceneAt(i);

                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Exporting dynamic objects in scene: {0}", scene.name), (float)i / (float)SceneManager.sceneCount);

                if (!scene.isLoaded)
                {
                    Debug.LogWarning(string.Format("Scene {0} is not loaded in the hierarchy.", scene.name));
                    continue;
                }

                ExportDynamicObjectsInArray(GetDynamicObjectsInScene(scene));
            }

            EditorUtility.DisplayProgressBar("Steam Audio", "", 1.0f);
            EditorUtility.ClearProgressBar();
        }

        [MenuItem("Steam Audio/Export Dynamic Objects In All Scenes In Build", false, 38)]
        public static void ExportDynamicObjectsInBuild()
        {
            for (var i = 0; i < SceneManager.sceneCountInBuildSettings; ++i)
            {
                var scene = SceneManager.GetSceneByBuildIndex(i);

                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Exporting dynamic objects in scene: {0}", scene.name), (float)i / (float)SceneManager.sceneCountInBuildSettings);

                var shouldClose = false;
                if (!scene.isLoaded)
                {
                    scene = EditorSceneManager.OpenScene(SceneUtility.GetScenePathByBuildIndex(i), OpenSceneMode.Additive);
                    shouldClose = true;
                }

                ExportDynamicObjectsInArray(GetDynamicObjectsInScene(scene));

                if (shouldClose)
                {
                    EditorSceneManager.CloseScene(scene, true);
                }
            }

            EditorUtility.DisplayProgressBar("Steam Audio", "", 1.0f);
            EditorUtility.ClearProgressBar();
        }

        [MenuItem("Steam Audio/Export All Dynamic Objects In Project", false, 39)]
        public static void ExportDynamicObjectsInProject()
        {
            var scenes = AssetDatabase.FindAssets("t:Scene");
            var prefabs = AssetDatabase.FindAssets("t:Prefab");

            var numItems = scenes.Length + prefabs.Length;

            var index = 0;
            foreach (var sceneGUID in scenes)
            {
                var scenePath = AssetDatabase.GUIDToAssetPath(sceneGUID);

                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Exporting dynamic objects in scene: {0}", scenePath), (float)index / (float)numItems);

                var activeScene = EditorSceneManager.GetActiveScene();
                var isLoadedScene = (scenePath == activeScene.path);

                var scene = activeScene;
                if (!isLoadedScene)
                {
#if UNITY_2019_2_OR_NEWER
                    var packageInfo = UnityEditor.PackageManager.PackageInfo.FindForAssetPath(scenePath);
                    if (!(packageInfo == null || packageInfo.source == PackageSource.Embedded || packageInfo.source == PackageSource.Local))
                    {
                        Debug.LogWarning(string.Format("Scene {0} is part of a read-only package, skipping.", scenePath));
                        continue;
                    }
#endif

                    scene = EditorSceneManager.OpenScene(scenePath, OpenSceneMode.Additive);
                }

                ExportDynamicObjectsInArray(GetDynamicObjectsInScene(scene));

                if (!isLoadedScene)
                {
                    EditorSceneManager.CloseScene(scene, true);
                }

                ++index;
            }

            foreach (var prefabGUID in prefabs)
            {
                var prefabPath = AssetDatabase.GUIDToAssetPath(prefabGUID);

                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Exporting dynamic objects in prefab: {0}", prefabPath), (float)index / (float)numItems);

                var prefab = AssetDatabase.LoadMainAssetAtPath(prefabPath) as GameObject;
                var dynamicObjects = prefab.GetComponentsInChildren<SteamAudioDynamicObject>();
                ExportDynamicObjectsInArray(dynamicObjects);

                ++index;
            }

            EditorUtility.DisplayProgressBar("Steam Audio", "", 1.0f);
            EditorUtility.ClearProgressBar();
        }

        [MenuItem("Steam Audio/Install FMOD Studio Plugin Files", false, 50)]
        public static void InstallFMODStudioPluginFiles()
        {
            // Make sure the FMOD Studio Unity integration is installed.
            var assemblySuffix = ",FMODUnity";
            var FMODUnity_Settings = Type.GetType("FMODUnity.Settings" + assemblySuffix);
            if (FMODUnity_Settings == null)
            {
                EditorUtility.DisplayDialog("Steam Audio",
                    "The FMOD Studio Unity integration does not seem to be installed to your Unity project. Install " +
                    "it and try again.",
                    "OK");
                return;
            }

            // Make sure we're using at least FMOD Studio v2.0.
            var FMODUnity_Settings_Instance = FMODUnity_Settings.GetProperty("Instance");
            var FMODUnity_Settings_CurrentVersion = FMODUnity_Settings.GetField("CurrentVersion");
            var fmodSettings = FMODUnity_Settings_Instance.GetValue(null, null);
            var fmodVersion = (int)FMODUnity_Settings_CurrentVersion.GetValue(fmodSettings);
            var fmodVersionMajor = (fmodVersion & 0x00ff0000) >> 16;
            var fmodVersionMinor = (fmodVersion & 0x0000ff00) >> 8;
            var fmodVersionPatch = (fmodVersion & 0x000000ff);
            if (fmodVersionMajor < 2)
            {
                EditorUtility.DisplayDialog("Steam Audio",
                    "Steam Audio requires FMOD Studio 2.0 or later.",
                    "OK");
                return;
            }

            var moveRequired = false;
            var moveSucceeded = false;

            // Look for the FMOD Studio plugin files. The files are in the right place for FMOD Studio 2.2
            // out of the box, but will need to be copied for 2.1 or earlier.
            // 2.0 through 2.1 expect plugin files in Assets/Plugins/FMOD/lib/(platform)
            // 2.2 expects plugin files in Assets/Plugins/FMOD/platforms/(platform)/lib
            if (AssetExists("Assets/Plugins/FMOD/lib/win/x86_64/phonon_fmod.dll"))
            {
                // Files are in the location corresponding to 2.1 or earlier.
                if (fmodVersionMinor >= 2)
                {
                    // We're using 2.2 or later, so we need to move files.
                    moveRequired = true;

                    var moves = new Dictionary<string, string>();
                    moves.Add("Assets/Plugins/FMOD/lib/win/x86/phonon_fmod.dll", "Assets/Plugins/FMOD/platforms/win/lib/x86/phonon_fmod.dll");
                    moves.Add("Assets/Plugins/FMOD/lib/win/x86_64/phonon_fmod.dll", "Assets/Plugins/FMOD/platforms/win/lib/x86_64/phonon_fmod.dll");
                    moves.Add("Assets/Plugins/FMOD/lib/linux/x86/libphonon_fmod.so", "Assets/Plugins/FMOD/platforms/linux/lib/x86/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/lib/linux/x86_64/libphonon_fmod.so", "Assets/Plugins/FMOD/platforms/linux/lib/x86_64/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/lib/mac/phonon_fmod.bundle", "Assets/Plugins/FMOD/platforms/mac/lib/phonon_fmod.bundle");
                    moves.Add("Assets/Plugins/FMOD/lib/android/armeabi-v7a/libphonon_fmod.so", "Assets/Plugins/FMOD/platforms/android/lib/armeabi-v7a/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/lib/android/arm64-v8a/libphonon_fmod.so", "Assets/Plugins/FMOD/platforms/android/lib/arm64-v8a/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/lib/android/x86/libphonon_fmod.so", "Assets/Plugins/FMOD/platforms/android/lib/x86/libphonon_fmod.so");

                    moveSucceeded = MoveAssets(moves);
                }
            }
            else if (AssetExists("Assets/Plugins/FMOD/platforms/win/lib/x86_64/phonon_fmod.dll"))
            {
                // Files are in the location corresponding to 2.2 or later.
                if (fmodVersionMinor <= 1)
                {
                    // We're using 2.1 or earlier, so we need to move files.
                    moveRequired = true;

                    var moves = new Dictionary<string, string>();
                    moves.Add("Assets/Plugins/FMOD/platforms/win/lib/x86/phonon_fmod.dll", "Assets/Plugins/FMOD/lib/win/x86/phonon_fmod.dll");
                    moves.Add("Assets/Plugins/FMOD/platforms/win/lib/x86_64/phonon_fmod.dll", "Assets/Plugins/FMOD/lib/win/x86_64/phonon_fmod.dll");
                    moves.Add("Assets/Plugins/FMOD/platforms/linux/lib/x86/libphonon_fmod.so", "Assets/Plugins/FMOD/lib/linux/x86/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/platforms/linux/lib/x86_64/libphonon_fmod.so", "Assets/Plugins/FMOD/lib/linux/x86_64/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/platforms/mac/lib/phonon_fmod.bundle", "Assets/Plugins/FMOD/lib/mac/phonon_fmod.bundle");
                    moves.Add("Assets/Plugins/FMOD/platforms/android/lib/armeabi-v7a/libphonon_fmod.so", "Assets/Plugins/FMOD/lib/android/armeabi-v7a/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/platforms/android/lib/arm64-v8a/libphonon_fmod.so", "Assets/Plugins/FMOD/lib/android/arm64-v8a/libphonon_fmod.so");
                    moves.Add("Assets/Plugins/FMOD/platforms/android/lib/x86/libphonon_fmod.so", "Assets/Plugins/FMOD/lib/android/x86/libphonon_fmod.so");

                    moveSucceeded = MoveAssets(moves);
                }
            }
            else
            {
                EditorUtility.DisplayDialog("Steam Audio",
                    "Unable to find Steam Audio FMOD Studio plugin files. Try reinstalling the Steam Audio Unity " +
                    "integration.",
                    "OK");
                return;
            }

            if (!moveRequired)
            {
                EditorUtility.DisplayDialog("Steam Audio",
                    "Steam Audio FMOD Studio plugin files are already in the correct place.",
                    "OK");
            }
            else if (!moveSucceeded)
            {
                EditorUtility.DisplayDialog("Steam Audio",
                    "Failed to copy Steam Audio FMOD Studio plugin files to the correct place. See the console for " +
                    "details.",
                    "OK");
            }
            else
            {
                EditorUtility.DisplayDialog("Steam Audio",
                    "Steam Audio FMOD Studio plugin files moved to the correct place.",
                    "OK");
            }
        }

        [MenuItem("Steam Audio/Install FMOD Studio Plugin Files", true)]
        public static bool ValidateInstallFMODStudioPluginFiles()
        {
            return (SteamAudioSettings.Singleton.audioEngine == AudioEngineType.FMODStudio);
        }

        private static bool AssetExists(string assetPath)
        {
            return !string.IsNullOrEmpty(AssetDatabase.AssetPathToGUID(assetPath)) &&
                (File.Exists(Environment.CurrentDirectory + "/" + assetPath) || Directory.Exists(Environment.CurrentDirectory + "/" + assetPath));
        }

        private static bool EnsureAssetDirectoryExists(string directory)
        {
            if (AssetDatabase.IsValidFolder(directory))
                return true;

            var parent = Path.GetDirectoryName(directory);
            var baseName = Path.GetFileName(directory);

            if (!EnsureAssetDirectoryExists(parent))
                return false;

            var result = AssetDatabase.CreateFolder(parent, baseName);
            if (string.IsNullOrEmpty(result))
            {
                Debug.LogErrorFormat("Unable to create asset directory {0} in {1}: {2}", baseName, parent, result);
                return false;
            }

            return true;
        }

        private static bool MoveAssets(Dictionary<string, string> moves)
        {
            foreach (var source in moves.Keys)
            {
                if (!AssetExists(source))
                {
                    Debug.LogErrorFormat("Unable to find plugin file: {0}", source);
                    return false;
                }

                var destination = moves[source];
                var directory = Path.GetDirectoryName(destination);

                if (!EnsureAssetDirectoryExists(directory))
                {
                    Debug.LogErrorFormat("Unable to create directory: {0}", directory);
                    return false;
                }

                var result = AssetDatabase.MoveAsset(source, destination);

                if (!string.IsNullOrEmpty(result))
                {
                    Debug.LogErrorFormat("Unable to move {0} to {1}: {2}", source, destination, result);
                    return false;
                }

                Debug.LogFormat("Moved {0} to {1}.", source, destination);
            }

            return true;
        }
#endif

        // Exports a dynamic object.
        public static void ExportDynamicObject(SteamAudioDynamicObject dynamicObject, bool exportOBJ)
        {
            var objects = GetDynamicGameObjectsForExport(dynamicObject);

            if (objects == null || objects.Length == 0)
            {
                Debug.LogError(string.Format("Dynamic object {0} has no Steam Audio geometry attached. Skipping export.", dynamicObject.name));
                return;
            }

            var dataAsset = (!exportOBJ) ? GetDataAsset(dynamicObject) : null;
            var objFileName = (exportOBJ) ? GetOBJFileName(dynamicObject) : "";

            Export(objects, dynamicObject.name, dataAsset, objFileName, true, exportOBJ);
        }

        // Exports all dynamic objects in an array.
        static void ExportDynamicObjectsInArray(SteamAudioDynamicObject[] dynamicObjects)
        {
            foreach (var dynamicObject in dynamicObjects)
            {
                ExportDynamicObject(dynamicObject, false);
            }
        }

        // Finds all dynamic objects in a scene.
        static SteamAudioDynamicObject[] GetDynamicObjectsInScene(UnityEngine.SceneManagement.Scene scene)
        {
            var dynamicObjects = new List<SteamAudioDynamicObject>();

            var rootObjects = scene.GetRootGameObjects();
            foreach (var rootObject in rootObjects)
            {
                dynamicObjects.AddRange(rootObject.GetComponentsInChildren<SteamAudioDynamicObject>());
            }

            return dynamicObjects.ToArray();
        }

        // Loads a static scene.
        public static void LoadScene(UnityEngine.SceneManagement.Scene unityScene, Context context, bool additive)
        {
            if (!additive)
            {
                sSingleton.mCurrentScene = CreateScene(context);
            }
        }

        // Loads a dynamic object as an instanced mesh. Multiple dynamic objects loaded from the same file
        // will share the underlying geometry and material data (using a reference count). The instanced meshes
        // allow each dynamic object to have its own transform.
        public static InstancedMesh LoadDynamicObject(SteamAudioDynamicObject dynamicObject, Scene parentScene, Context context)
        {
            InstancedMesh instancedMesh = null;

            var dataAsset = dynamicObject.asset;
            var assetName = dataAsset.name;
            if (dataAsset != null)
            {
                Scene subScene = null;
                if (sSingleton.mDynamicObjects.ContainsKey(assetName))
                {
                    subScene = sSingleton.mDynamicObjects[assetName];
                    sSingleton.mDynamicObjectRefCounts[assetName]++;
                }
                else
                {
                    subScene = CreateScene(context);
                    var subStaticMesh = Load(dataAsset, context, subScene);
                    subStaticMesh.AddToScene(subScene);
                    subStaticMesh.Release();

                    sSingleton.mDynamicObjects.Add(assetName, subScene);
                    sSingleton.mDynamicObjectRefCounts.Add(assetName, 1);
                }

                instancedMesh = new InstancedMesh(parentScene, subScene, dynamicObject.transform);
            }

            return instancedMesh;
        }

        // Unloads a dynamic object and decrements the reference count of the underlying data. However,
        // when the reference count hits zero, we don't get rid of the data, because the dynamic object may
        // be instantiated again within a few frames, and we don't want to waste time re-loading it. The data
        // will eventually be unloaded at the next scene change.
        public static void UnloadDynamicObject(SteamAudioDynamicObject dynamicObject)
        {
            var assetName = (dynamicObject.asset) ? dynamicObject.asset.name : "";

            if (sSingleton.mDynamicObjectRefCounts.ContainsKey(assetName))
            {
                sSingleton.mDynamicObjectRefCounts[assetName]--;
            }
        }

        // Gather a list of all GameObjects to export, starting from a given root object.
        public static List<GameObject> GetGameObjectsForExport(GameObject root, bool exportingStaticObjects = false)
        {
            var gameObjects = new List<GameObject>();

            if (exportingStaticObjects && root.GetComponentInParent<SteamAudioDynamicObject>() != null)
                return new List<GameObject>();

            var geometries = root.GetComponentsInChildren<SteamAudioGeometry>();
            foreach (var geometry in geometries)
            {
                if (IsDynamicSubObject(root, geometry.gameObject))
                    continue;

                if (geometry.exportAllChildren)
                {
                    var meshes = geometry.GetComponentsInChildren<MeshFilter>();
                    foreach (var mesh in meshes)
                    {
                        if (!IsDynamicSubObject(root, mesh.gameObject))
                        {
                            if (IsActiveInHierarchy(mesh.gameObject.transform))
                            {
                                gameObjects.Add(mesh.gameObject);
                            }
                        }
                    }

                    var terrains = geometry.GetComponentsInChildren<Terrain>();
                    foreach (var terrain in terrains)
                    {
                        if (!IsDynamicSubObject(root, terrain.gameObject))
                        {
                            if (IsActiveInHierarchy(terrain.gameObject.transform))
                            {
                                gameObjects.Add(terrain.gameObject);
                            }
                        }
                    }
                }
                else
                {
                    if (IsActiveInHierarchy(geometry.gameObject.transform))
                    {
                        if (geometry.gameObject.GetComponent<MeshFilter>() != null ||
                            geometry.gameObject.GetComponent<Terrain>() != null)
                        {
                            gameObjects.Add(geometry.gameObject);
                        }
                    }
                }
            }

            var uniqueGameObjects = new HashSet<GameObject>(gameObjects);

            gameObjects.Clear();
            foreach (var uniqueGameObject in uniqueGameObjects)
            {
                gameObjects.Add(uniqueGameObject);
            }

            return gameObjects;
        }

        // Returns the number of vertices associated with a GameObject.
        public static int GetNumVertices(GameObject gameObject)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null)
            {
                return mesh.sharedMesh.vertexCount;
            }
            else if (terrain != null)
            {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapResolution;
                var h = terrain.terrainData.heightmapResolution;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));

                if (s == 0)
                {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                return (w * h);
            }
            else
            {
                return 0;
            }
        }

        // Returns the number of triangles associated with a GameObject.
        public static int GetNumTriangles(GameObject gameObject)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null)
            {
                return mesh.sharedMesh.triangles.Length / 3;
            }
            else if (terrain != null)
            {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapResolution;
                var h = terrain.terrainData.heightmapResolution;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));

                if (s == 0)
                {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                return ((w - 1) * (h - 1) * 2);
            }
            else
            {
                return 0;
            }
        }

        [MonoPInvokeCallback(typeof(ClosestHitCallback))]
        public static void ClosestHit(ref Ray ray, float minDistance, float maxDistance, out Hit hit, IntPtr userData)
        {
            var origin = Common.ConvertVector(ray.origin);
            var direction = Common.ConvertVector(ray.direction);

            origin += minDistance * direction;

            var layerMask = SteamAudioSettings.Singleton.layerMask;

            hit.objectIndex = 0;
            hit.triangleIndex = 0;
            hit.materialIndex = 0;

            var numHits = Physics.RaycastNonAlloc(origin, direction, sSingleton.mRayHits, maxDistance, layerMask);
            if (numHits > 0)
            {
                hit.distance = sSingleton.mRayHits[0].distance;
                hit.normal = Common.ConvertVector(sSingleton.mRayHits[0].normal);
                hit.material = GetMaterialBufferForTransform(sSingleton.mRayHits[0].collider.transform);
            }
            else
            {
                hit.distance = Mathf.Infinity;
                hit.normal = new Vector3 { x = 0.0f, y = 0.0f, z = 0.0f };
                hit.material = IntPtr.Zero;
            }
        }

        [MonoPInvokeCallback(typeof(AnyHitCallback))]
        public static void AnyHit(ref Ray ray, float minDistance, float maxDistance, out byte occluded, IntPtr userData)
        {
            var origin = Common.ConvertVector(ray.origin);
            var direction = Common.ConvertVector(ray.direction);

            origin += minDistance * direction;

            var layerMask = SteamAudioSettings.Singleton.layerMask;

            var numHits = Physics.RaycastNonAlloc(origin, direction, sSingleton.mRayHits, maxDistance, layerMask);

            occluded = (byte)((numHits > 0) ? 1 : 0);
        }

        // This method is called as soon as scripts are loaded, which happens whenever play mode is started
        // (in the editor), or whenever the game is launched. We then create a Steam Audio Manager object
        // and move it to the Don't Destroy On Load list.
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void AutoInitialize()
        {
            Initialize(ManagerInitReason.Playing);
        }

        // Exports the static geometry in a scene.
        public static void ExportScene(UnityEngine.SceneManagement.Scene unityScene, bool exportOBJ)
        {
            var objects = GetStaticGameObjectsForExport(unityScene);

            if (objects == null || objects.Length == 0)
            {
                Debug.LogWarning(string.Format("Scene {0} has no Steam Audio static geometry. Skipping export.", unityScene.name));
                return;
            }

            var dataAsset = (!exportOBJ) ? GetDataAsset(unityScene) : null;
            var objFileName = (exportOBJ) ? GetOBJFileName(unityScene) : "";

            if (!exportOBJ && dataAsset == null)
                return;

            if (exportOBJ && (objFileName == null || objFileName.Length == 0))
                return;

            Export(objects, unityScene.name, dataAsset, objFileName, false, exportOBJ);
        }

        // Exports a set of GameObjects.
        static void Export(GameObject[] objects, string name, SerializedData dataAsset, string objFileName, bool dynamic, bool exportOBJ)
        {
            var type = (dynamic) ? "Dynamic Object" : "Scene";

            Vector3[] vertices = null;
            Triangle[] triangles = null;
            int[] materialIndices = null;
            Material[] materials = null;
            GetGeometryAndMaterialBuffers(objects, ref vertices, ref triangles, ref materialIndices, ref materials, dynamic, exportOBJ);

            if (vertices.Length == 0 || triangles.Length == 0 || materialIndices.Length == 0 || materials.Length == 0)
            {
                Debug.LogError(string.Format("Steam Audio {0} [{1}]: No Steam Audio Geometry components attached.", type, name));
                return;
            }

            var context = new Context();

            // Scene type should always be Phonon when exporting.
            var scene = new Scene(context, SceneType.Default, null, null, null, null);

            var staticMesh = new StaticMesh(context, scene, vertices, triangles, materialIndices, materials);
            staticMesh.AddToScene(scene);

            if (exportOBJ)
            {
                scene.Commit();
                scene.SaveOBJ(objFileName);
            }
            else
            {
                staticMesh.Save(dataAsset);
            }

            Debug.Log(string.Format("Steam Audio {0} [{1}]: Exported to {2}.", type, name, (exportOBJ) ? objFileName : dataAsset.name));

            staticMesh.Release();
            scene.Release();
        }

        static Scene CreateScene(Context context)
        {
            var sceneType = GetSceneType();

            var scene = new Scene(context, sceneType, sSingleton.mEmbreeDevice, sSingleton.mRadeonRaysDevice,
                ClosestHit, AnyHit);

            if (sceneType == SceneType.Custom)
            {
                if (sSingleton.mMaterialBuffer == IntPtr.Zero)
                {
                    sSingleton.mMaterialBuffer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Material)));
                }
            }

            return scene;
        }

        // Loads a Steam Audio scene.
        static StaticMesh Load(SerializedData dataAsset, Context context, Scene scene)
        {
            return new StaticMesh(context, scene, dataAsset);
        }

        // Unloads the underlying data for dynamic objects. Can either remove only unreferenced data (for use when
        // changing scenes) or all data (for use when shutting down).
        static void RemoveAllDynamicObjects(bool force = false)
        {
            var unreferencedDynamicObjects = new List<string>();

            foreach (var scene in sSingleton.mDynamicObjectRefCounts.Keys)
            {
                if (force || sSingleton.mDynamicObjectRefCounts[scene] == 0)
                {
                    unreferencedDynamicObjects.Add(scene);
                }
            }

            foreach (var scene in unreferencedDynamicObjects)
            {
                sSingleton.mDynamicObjects[scene].Release();
                sSingleton.mDynamicObjects.Remove(scene);
                sSingleton.mDynamicObjectRefCounts.Remove(scene);
            }
        }

        // Unloads all currently-loaded scenes.
        static void RemoveAllAdditiveScenes()
        {
            Marshal.FreeHGlobal(sSingleton.mMaterialBuffer);

            if (sSingleton.mCurrentScene != null)
            {
                sSingleton.mCurrentScene.Release();
                sSingleton.mCurrentScene = null;
            }
        }

        static IntPtr GetMaterialBufferForTransform(Transform obj)
        {
            var material = new Material();
            var found = false;

            var currentObject = obj;
            while (currentObject != null)
            {
                var steamAudioGeometry = currentObject.GetComponent<SteamAudioGeometry>();
                if (steamAudioGeometry != null && steamAudioGeometry.material != null)
                {
                    material = steamAudioGeometry.material.GetMaterial();
                    found = true;
                    break;
                }
                currentObject = currentObject.parent;
            }

            if (!found)
            {
                material = SteamAudioSettings.Singleton.defaultMaterial.GetMaterial();
            }

            Marshal.StructureToPtr(material, sSingleton.mMaterialBuffer, true);

            return sSingleton.mMaterialBuffer;
        }

        // Gather a list of all GameObjects to export in a scene, excluding dynamic objects.
        static GameObject[] GetStaticGameObjectsForExport(UnityEngine.SceneManagement.Scene scene)
        {
            var gameObjects = new List<GameObject>();

            var roots = scene.GetRootGameObjects();
            foreach (var root in roots)
            {
                gameObjects.AddRange(GetGameObjectsForExport(root, true));
            }

            return gameObjects.ToArray();
        }

        // Gather a list of all GameObjects to export for a given dynamic object.
        static GameObject[] GetDynamicGameObjectsForExport(SteamAudioDynamicObject dynamicObject)
        {
            return GetGameObjectsForExport(dynamicObject.gameObject).ToArray();
        }

        static bool IsDynamicSubObject(GameObject root, GameObject obj)
        {
            return (root.GetComponentInParent<SteamAudioDynamicObject>() !=
                obj.GetComponentInParent<SteamAudioDynamicObject>());
        }

        // Ideally, we want to use GameObject.activeInHierarchy to check if a GameObject is active. However, when
        // we batch-export dynamic objects, Prefabs are instantiated using AssetDatabase.LoadMainAssetAtPath,
        // and isActiveInHierarchy returns false even if all GameObjects in the Prefab return true for
        // GameObject.activeSelf. Therefore, we manually walk up the hierarchy and check if the GameObject is active.
        static bool IsActiveInHierarchy(Transform obj)
        {
            if (obj == null)
                return true;

            return (obj.gameObject.activeSelf && IsActiveInHierarchy(obj.parent));
        }

        // Given an array of GameObjects, export the vertex, triangle, material index, and material data.
        static void GetGeometryAndMaterialBuffers(GameObject[] gameObjects, ref Vector3[] vertices, ref Triangle[] triangles, ref int[] materialIndices, ref Material[] materials, bool isDynamic, bool exportOBJ)
        {
            var numVertices = new int[gameObjects.Length];
            var numTriangles = new int[gameObjects.Length];
            var totalNumVertices = 0;
            var totalNumTriangles = 0;
            for (var i = 0; i < gameObjects.Length; ++i)
            {
                numVertices[i] = GetNumVertices(gameObjects[i]);
                numTriangles[i] = GetNumTriangles(gameObjects[i]);
                totalNumVertices += numVertices[i];
                totalNumTriangles += numTriangles[i];
            }

            int[] materialIndicesPerObject = null;
            GetMaterialMapping(gameObjects, ref materials, ref materialIndicesPerObject);

            vertices = new Vector3[totalNumVertices];
            triangles = new Triangle[totalNumTriangles];
            materialIndices = new int[totalNumTriangles];

            // If we're exporting a dynamic object, apply the relevant transform. However, if we're exporting
            // to an OBJ file, _don't_ apply the transform, so the dynamic object appears centered at its local
            // origin.
            Transform transform = null;
            if (isDynamic && !exportOBJ)
            {
                var dynamicObject = gameObjects[0].GetComponent<SteamAudioDynamicObject>();
                if (dynamicObject == null)
                {
                    dynamicObject = GetDynamicObjectInParent(gameObjects[0].transform);
                }
                transform = dynamicObject.transform;
            }

            var verticesOffset = 0;
            var trianglesOffset = 0;
            for (var i = 0; i < gameObjects.Length; ++i)
            {
                GetVertices(gameObjects[i], vertices, verticesOffset, transform);
                GetTriangles(gameObjects[i], triangles, trianglesOffset);
                FixupTriangleIndices(triangles, trianglesOffset, trianglesOffset + numTriangles[i], verticesOffset);

                for (var j = 0; j < numTriangles[i]; ++j)
                {
                    materialIndices[trianglesOffset + j] = materialIndicesPerObject[i];
                }

                verticesOffset += numVertices[i];
                trianglesOffset += numTriangles[i];
            }
        }

        // Ideally, we want to use GameObject.GetComponentInParent<>() to find the SteamAudioDynamicObject attached to
        // an ancestor of this GameObject. However, GetComponentInParent only returns "active" components, which in
        // turn seem to be subject to the same behavior as activeInHierarchy (see above), so we have to manually walk
        // the hierarchy upwards to find the first SteamAudioDynamicObject.
        static SteamAudioDynamicObject GetDynamicObjectInParent(Transform obj)
        {
            if (obj == null)
                return null;

            var dynamicObject = obj.gameObject.GetComponent<SteamAudioDynamicObject>();
            if (dynamicObject != null)
                return dynamicObject;

            return GetDynamicObjectInParent(obj.parent);
        }

        // Populates an array with the vertices associated with a GameObject, starting at a given offset.
        static void GetVertices(GameObject gameObject, Vector3[] vertices, int offset, Transform transform)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null)
            {
                var vertexArray = mesh.sharedMesh.vertices;
                for (var i = 0; i < vertexArray.Length; ++i)
                {
                    var transformedVertex = mesh.transform.TransformPoint(vertexArray[i]);
                    if (transform != null)
                    {
                        transformedVertex = transform.InverseTransformPoint(transformedVertex);
                    }
                    vertices[offset + i] = Common.ConvertVector(transformedVertex);
                }
            }
            else if (terrain != null)
            {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapResolution;
                var h = terrain.terrainData.heightmapResolution;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));
                if (s == 0)
                {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                var heights = terrain.terrainData.GetHeights(0, 0, terrain.terrainData.heightmapResolution,
                    terrain.terrainData.heightmapResolution);

                var index = 0;
                for (var v = 0; v < terrain.terrainData.heightmapResolution; v += s)
                {
                    for (var u = 0; u < terrain.terrainData.heightmapResolution; u += s)
                    {
                        var height = heights[v, u];

                        var x = ((float) u / terrain.terrainData.heightmapResolution) * terrain.terrainData.size.x;
                        var y = height * terrain.terrainData.size.y;
                        var z = ((float) v / terrain.terrainData.heightmapResolution) * terrain.terrainData.size.z;

                        var vertex = new UnityEngine.Vector3 { x = x, y = y, z = z };
                        var transformedVertex = terrain.transform.TransformPoint(vertex);
                        if (transform != null)
                        {
                            transformedVertex = transform.InverseTransformPoint(transformedVertex);
                        }
                        vertices[offset + index] = Common.ConvertVector(transformedVertex);
                        ++index;
                    }
                }
            }
        }

        // Populates an array with the triangles associated with a GameObject, starting at a given offset.
        static void GetTriangles(GameObject gameObject, Triangle[] triangles, int offset)
        {
            var mesh = gameObject.GetComponent<MeshFilter>();
            var terrain = gameObject.GetComponent<Terrain>();

            if (mesh != null)
            {
                var triangleArray = mesh.sharedMesh.triangles;
                for (var i = 0; i < triangleArray.Length / 3; ++i)
                {
                    triangles[offset + i].index0 = triangleArray[3 * i + 0];
                    triangles[offset + i].index1 = triangleArray[3 * i + 1];
                    triangles[offset + i].index2 = triangleArray[3 * i + 2];
                }
            }
            else if (terrain != null)
            {
                var terrainSimplificationLevel = GetTerrainSimplificationLevel(terrain);

                var w = terrain.terrainData.heightmapResolution;
                var h = terrain.terrainData.heightmapResolution;
                var s = Mathf.Min(w - 1, Mathf.Min(h - 1, (int)Mathf.Pow(2.0f, terrainSimplificationLevel)));
                if (s == 0)
                {
                    s = 1;
                }

                w = ((w - 1) / s) + 1;
                h = ((h - 1) / s) + 1;

                var index = 0;
                for (var v = 0; v < h - 1; ++v)
                {
                    for (var u = 0; u < w - 1; ++u)
                    {
                        var i0 = v * w + u;
                        var i1 = (v + 1) * w + u;
                        var i2 = v * w + (u + 1);
                        triangles[offset + index] = new Triangle
                        {
                            index0 = i0,
                            index1 = i1,
                            index2 = i2
                        };

                        i0 = v * w + (u + 1);
                        i1 = (v + 1) * w + u;
                        i2 = (v + 1) * w + (u + 1);
                        triangles[offset + index + 1] = new Triangle
                        {
                            index0 = i0,
                            index1 = i1,
                            index2 = i2
                        };

                        index += 2;
                    }
                }
            }
        }

        // When multiple meshes are combined to form a single piece of geometry, each mesh will have
        // 0-based triangle indices, even though the combined mesh will have a single vertex buffer. This
        // function applies appropriate offsets to triangle indices so make all vertex indices correct.
        static void FixupTriangleIndices(Triangle[] triangles, int startIndex, int endIndex, int indexOffset)
        {
            for (var i = startIndex; i < endIndex; ++i)
            {
                triangles[i].index0 += indexOffset;
                triangles[i].index1 += indexOffset;
                triangles[i].index2 += indexOffset;
            }
        }

        static float GetTerrainSimplificationLevel(Terrain terrain)
        {
            return terrain.GetComponentInParent<SteamAudioGeometry>().terrainSimplificationLevel;
        }

        // Given an array of GameObjects, returns: a) an array containing all the unique materials referenced by
        // them, and b) an array indicating for each GameObject, which material it references.
        static void GetMaterialMapping(GameObject[] gameObjects, ref Material[] materials, ref int[] materialIndices)
        {
            var materialMapping = new Dictionary<Material, List<int>>();

            // Loop through all the given GameObjects, and generate a dictionary mapping each material
            // to a list of GameObjects that reference it.
            for (var i = 0; i < gameObjects.Length; ++i)
            {
                var material = GetMaterialForGameObject(gameObjects[i]);
                if (!materialMapping.ContainsKey(material))
                {
                    materialMapping.Add(material, new List<int>());
                }
                materialMapping[material].Add(i);
            }

            materials = new Material[materialMapping.Keys.Count];
            materialIndices = new int[gameObjects.Length];

            // Extract an array of unique materials and an array mapping GameObjects to materials.
            var index = 0;
            foreach (var material in materialMapping.Keys)
            {
                materials[index] = material;
                foreach (var gameObjectIndex in materialMapping[material])
                {
                    materialIndices[gameObjectIndex] = index;
                }
                ++index;
            }
        }

        // Returns the Steam Audio material associated with a given GameObject.
        static Material GetMaterialForGameObject(GameObject gameObject)
        {
            // Traverse the hierarchy upwards starting at this GameObject, until we find the
            // first GameObject that has a Steam Audio Geometry component with a non-empty
            // Material property.
            var current = gameObject.transform;
            while (current != null)
            {
                var geometry = current.gameObject.GetComponent<SteamAudioGeometry>();
                if (geometry != null && geometry.material != null)
                {
                    return geometry.material.GetMaterial();
                }

                current = current.parent;
            }

            // If we didn't find any such GameObject, use the default material specified in
            // the Steam Audio Settings.
            var defaultMaterial = SteamAudioSettings.Singleton.defaultMaterial;
            if (defaultMaterial != null)
            {
                return SteamAudioSettings.Singleton.defaultMaterial.GetMaterial();
            }

            // The default material was set to null, so create a default material and use it.
            Debug.LogWarning(
                "A default material has not been set, using built-in default. Click Steam Audio > Settings " +
                "to specify a default material.");
            return ScriptableObject.CreateInstance<SteamAudioMaterial>().GetMaterial();
        }

        static string GetOBJFileName(UnityEngine.SceneManagement.Scene scene)
        {
            var fileName = "";

#if UNITY_EDITOR
            fileName = EditorUtility.SaveFilePanelInProject("Export Scene to OBJ", scene.name, "obj",
                "Select a file to export this scene's data to.");
#endif

            return fileName;
        }

        static string GetOBJFileName(SteamAudioDynamicObject dynamicObject)
        {
            var fileName = "";

#if UNITY_EDITOR
            fileName = EditorUtility.SaveFilePanelInProject("Export Dynamic Object to OBJ", dynamicObject.name, "obj",
                "Select a file to export this dynamic object's data to.");
#endif

            return fileName;
        }

        static SerializedData GetDataAsset(UnityEngine.SceneManagement.Scene scene)
        {
            SteamAudioStaticMesh steamAudioStaticMesh = null;
            var rootObjects = scene.GetRootGameObjects();
            foreach (var rootObject in rootObjects)
            {
                steamAudioStaticMesh = rootObject.GetComponentInChildren<SteamAudioStaticMesh>();
                if (steamAudioStaticMesh != null)
                    break;
            }

            if (steamAudioStaticMesh == null)
            {
                var activeScene = SceneManager.GetActiveScene();
                SceneManager.SetActiveScene(scene);
                var rootObject = new GameObject("Steam Audio Static Mesh");
                steamAudioStaticMesh = rootObject.AddComponent<SteamAudioStaticMesh>();
#if UNITY_EDITOR
                EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
#endif
                SceneManager.SetActiveScene(activeScene);
            }

            if (steamAudioStaticMesh.asset == null)
            {
                steamAudioStaticMesh.asset = SerializedData.PromptForNewAsset(scene.name);
                steamAudioStaticMesh.sceneNameWhenExported = scene.name;
            }

            return steamAudioStaticMesh.asset;
        }

        static SerializedData GetDataAsset(SteamAudioDynamicObject dynamicObject)
        {
            return dynamicObject.asset;
        }
#endif
    }
}
