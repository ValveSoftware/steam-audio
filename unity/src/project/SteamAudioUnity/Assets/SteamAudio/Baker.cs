//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEngine.SceneManagement;

using System;
using System.Runtime.InteropServices;
using System.Threading;

#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.SceneManagement;
#endif

namespace SteamAudio
{
    public enum BakingMode
    {
        StaticSource,
        StaticListener,
        Reverb,
    }

    //
    //    BakeStatus
    //    Possible states the bake process can be in.
    //
    public enum BakeStatus
    {
        Ready,
        InProgress,
        Complete
    }

    public class Baker
    {
        public void BakeEffectThread()
        {
            BakingSettings bakeSettings;
            bakeSettings.bakeConvolution = bakeConvolution ? Bool.True : Bool.False;
            bakeSettings.bakeParametric = bakeParameteric ? Bool.True : Bool.False;

#if UNITY_EDITOR
            totalObjects = duringBakeObjects.Length;
            objectBakingCurrently = 0;
#endif

            for (int i = 0; i < duringBakeObjects.Length; ++i)
            {
                bakingGameObjectName = "reverb";
                var bakeDataString = Common.ConvertString("__reverb__");
                var bakeDataStringWithPrefix = bakeDataString;
                var bakeDataPrefix = "";

                if (duringBakeModes[i] == BakingMode.StaticListener)
                {
                    bakeDataString = Common.HashStringForIdentifier(duringBakeIdentifiers[i]);
                    bakeDataStringWithPrefix = Common.HashStringForIdentifierWithPrefix(duringBakeIdentifiers[i],
                        bakedListenerPrefix);
                    bakeDataPrefix = bakedListenerPrefix;
                    bakingGameObjectName = duringBakeIdentifiers[i];
                }
                else if (duringBakeModes[i] == BakingMode.StaticSource)
                {
                    bakeDataString = Common.HashStringForIdentifier(duringBakeIdentifiers[i]);
                    bakeDataStringWithPrefix = bakeDataString;
                    bakingGameObjectName = duringBakeIdentifiers[i];
                }

                Debug.Log("START: Baking effect for " + bakingGameObjectName + ".");
                ++objectBakingCurrently;

#if UNITY_EDITOR
                totalProbeBoxes = duringBakeProbeBoxes[i].Length;
#endif

                probeBoxBakingCurrently = 0;
                var atLeastOneProbeBoxHasProbes = false;
                foreach (SteamAudioProbeBox probeBox in duringBakeProbeBoxes[i])
                {
                    if (cancelBake)
                        return;

                    if (probeBox == null)
                    {
                        Debug.LogError("Probe Box specified in list of Probe Boxes to bake is null.");
                        continue;
                    }

                    if (probeBox.probeBoxData == null || probeBox.probeBoxData.Length == 0)
                    {
                        Debug.LogError("Skipping probe box, because probes have not been generated for it.");
                        probeBoxBakingCurrently++;
                        continue;
                    }

                    atLeastOneProbeBoxHasProbes = true;

                    IntPtr probeBoxPtr = IntPtr.Zero;
                    try
                    {
                        PhononCore.iplLoadProbeBox(probeBox.probeBoxData, probeBox.probeBoxData.Length,
                            ref probeBoxPtr);
                        probeBoxBakingCurrently++;
                    }
                    catch (Exception e)
                    {
                        Debug.LogError(e.Message);
                    }

                    var environment = steamAudioManager.GameEngineState().Environment().GetEnvironment();

                    if (duringBakeModes[i] == BakingMode.Reverb)
                    {
                        PhononCore.iplBakeReverb(environment, probeBoxPtr, bakeSettings, bakeCallback);
                    }
                    else if (duringBakeModes[i] == BakingMode.StaticListener)
                    {
                        PhononCore.iplBakeStaticListener(environment, probeBoxPtr, duringBakeSpheres[i],
                            bakeDataString, bakeSettings, bakeCallback);
                    }
                    else if (duringBakeModes[i] == BakingMode.StaticSource)
                    {
                        PhononCore.iplBakePropagation(environment, probeBoxPtr, duringBakeSpheres[i],
                            bakeDataString, bakeSettings, bakeCallback);
                    }

                    if (cancelBake)
                    {
                        PhononCore.iplDestroyProbeBox(ref probeBoxPtr);
                        Debug.Log("CANCELLED: Baking.");
                        return;
                    }

                    int probeBoxSize = PhononCore.iplSaveProbeBox(probeBoxPtr, null);
                    probeBox.probeBoxData = new byte[probeBoxSize];
                    PhononCore.iplSaveProbeBox(probeBoxPtr, probeBox.probeBoxData);

                    int probeBoxEffectSize = PhononCore.iplGetBakedDataSizeByName(probeBoxPtr, bakeDataStringWithPrefix);
                    probeBox.UpdateProbeDataMapping(duringBakeIdentifiers[i], bakeDataPrefix, probeBoxEffectSize);

                    PhononCore.iplDestroyProbeBox(ref probeBoxPtr);
                }

                if (duringBakeProbeBoxes[i].Length == 0)
                {
                    Debug.LogError("Probe Box component not attached or no probe boxes selected for "
                        + bakingGameObjectName);
                }
                else if (atLeastOneProbeBoxHasProbes)
                {
                    Debug.Log("COMPLETED: Baking effect for " + bakingGameObjectName + ".");
                }
            }

            bakeStatus = BakeStatus.Complete;
        }

        public void BeginBake(GameObject[] gameObjects, BakingMode[] bakingModes, string[] stringIdentifiers,
            Sphere[] influenceSpheres, SteamAudioProbeBox[][] probeBoxes)
        {
            duringBakeObjects = gameObjects;
            duringBakeProbeBoxes = probeBoxes;
            duringBakeModes = bakingModes;
            duringBakeIdentifiers = stringIdentifiers;
            duringBakeSpheres = influenceSpheres;

            for (int i=0; i<gameObjects.Length; ++i)
            {
                if (stringIdentifiers[i].Length == 0 && bakingModes[i] != BakingMode.Reverb)
                    Debug.LogError("Unique identifier not specified for GameObject " + gameObjects[i].name);
            }

            // Initialize environment
            try
            {
                steamAudioManager = GameObject.FindObjectOfType<SteamAudioManager>();
                if (steamAudioManager == null)
                    throw new Exception("Phonon Manager Settings object not found in the scene! Click Window > Phonon");

                steamAudioManager.Initialize(GameEngineStateInitReason.Baking);

                if (steamAudioManager.GameEngineState().Scene().GetScene() == IntPtr.Zero)
                    throw new Exception("Make sure to pre-export the scene before baking.");
            }
            catch (Exception e)
            {
                bakeStatus = BakeStatus.Complete;
                Debug.LogError(e.Message);
                return;
            }

            oneBakeActive = true;
            bakeStatus = BakeStatus.InProgress;

            bakeCallback = new PhononCore.BakeProgressCallback(AdvanceProgress);

#if (UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN)
            bakeCallbackPointer = Marshal.GetFunctionPointerForDelegate(bakeCallback);
            bakeCallbackHandle = GCHandle.Alloc(bakeCallbackPointer);
            GC.Collect();
#endif

#if UNITY_EDITOR
            EditorApplication.update += InEditorUpdate;
#endif

            bakeThread = new Thread(new ThreadStart(BakeEffectThread));
            bakeThread.Start();
        }

        public void EndBake()
        {
            if (bakeThread != null)
                bakeThread.Join();

#if (UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN)
            if (bakeCallbackHandle.IsAllocated)
                bakeCallbackHandle.Free();
#endif

            if (steamAudioManager)
                steamAudioManager.Destroy();

            steamAudioManager = null;
            duringBakeProbeBoxes = null;
#if UNITY_EDITOR
            totalProbeBoxes = 0;
            totalObjects = 0;
            probeBoxBakingProgress = .0f;
#endif
            probeBoxBakingCurrently = 0;
            bakeStatus = BakeStatus.Ready;

            oneBakeActive = false;
#if UNITY_EDITOR
            EditorApplication.update -= InEditorUpdate;
#endif
        }

        void InEditorUpdate()
        {
#if UNITY_EDITOR
            if (GetBakeStatus() == BakeStatus.Complete)
            {
                EndBake();
                EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
            }
#endif
        }

        public void CancelBake()
        {
            // Ensures partial baked data is not serialized and that bake is properly canceled for multiple 
            // probe boxes.
            cancelBake = true;
            PhononCore.iplCancelBake();
            EndBake();
            oneBakeActive = false;
            cancelBake = false;
        }

        public static bool IsBakeActive()
        {
            return oneBakeActive;
        }

        public BakeStatus GetBakeStatus()
        {
            return bakeStatus;
        }

        void AdvanceProgress(float bakeProgressFraction)
        {
#if UNITY_EDITOR
            probeBoxBakingProgress = bakeProgressFraction;
#endif
        }

        public bool DrawProgressBar()
        {
#if UNITY_EDITOR
            if (bakeStatus != BakeStatus.InProgress)
                return false;

            float progress = probeBoxBakingProgress + .01f; //Adding an offset because progress bar when it is exact 0 has some non-zero progress.
            int progressPercent = Mathf.FloorToInt(Mathf.Min(progress * 100.0f, 100.0f));
            string progressString =  bakingGameObjectName + ": Baking " + probeBoxBakingCurrently + "/" + 
                totalProbeBoxes + " Probe Box (" + progressPercent.ToString() + "% complete)";

            if (totalObjects > 1)
                progressString = "(" + objectBakingCurrently + "/" + totalObjects + " objects) " + progressString;

            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.PrefixLabel(" ");
            EditorGUI.ProgressBar(EditorGUILayout.GetControlRect(), progress, progressString);
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.PrefixLabel(" ");
            if (GUILayout.Button("Cancel Bake"))
            {
                CancelBake();
                return false;
            }
            EditorGUILayout.EndHorizontal();
#endif
            return true;
        }

        //Bake Settings
        static bool oneBakeActive = false;
        string bakingGameObjectName = "";
        int probeBoxBakingCurrently = 0;
        int objectBakingCurrently = 0;

#if UNITY_EDITOR
        int totalProbeBoxes = 0;
        int totalObjects = 0;
        float probeBoxBakingProgress = .0f;
#endif

        bool bakeConvolution = true;
        bool bakeParameteric = false;
        bool cancelBake = false;

        BakeStatus bakeStatus = BakeStatus.Ready;
        SteamAudioManager steamAudioManager = null;

        SteamAudioProbeBox[][] duringBakeProbeBoxes = null;
        Sphere[] duringBakeSpheres;
        BakingMode[] duringBakeModes;
        GameObject[] duringBakeObjects;
        string[] duringBakeIdentifiers;

        string bakedListenerPrefix = "__staticlistener__";

        Thread bakeThread;
        PhononCore.BakeProgressCallback bakeCallback;

#if (UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN)
        IntPtr bakeCallbackPointer;
        GCHandle bakeCallbackHandle;
#endif

    }
}

