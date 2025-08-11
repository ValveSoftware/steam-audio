//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#if STEAMAUDIO_ENABLED

using AOT;
using System;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;
using UnityEngine.SceneManagement;
#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.SceneManagement;
#endif

namespace SteamAudio
{
    public enum BakeStatus
    {
        Ready,
        InProgress,
        Complete
    }

    public struct BakedDataTask
    {
        public GameObject gameObject;
        public MonoBehaviour component;
        public string name;
        public BakedDataIdentifier identifier;
        public SteamAudioProbeBatch[] probeBatches;
        public string[] probeBatchNames;
        public SerializedData[] probeBatchAssets;
        public SteamAudioReverbDataPoint probe;
        public UnityEngine.Vector3 probePosition;
    }

    public static class Baker
    {
        static BakeStatus sStatus = BakeStatus.Ready;
#if UNITY_EDITOR
        static float sProgress = 0.0f;
        static int sProgressId = -1;
        static bool sShowModalProgressBar = false;
#endif
        static ProgressCallback sProgressCallback = null;
        static IntPtr sProgressCallbackPointer = IntPtr.Zero;
        static GCHandle sProgressCallbackHandle;
        static Thread sThread;

        static int sNumSubTasks = 0;
        static int sNumSubTasksCompleted = 0;

        static bool sCancel = false;
        static BakedDataTask[] sTasks = null;

        public static void BeginBake(BakedDataTask[] tasks, bool showModalProgressBar = false)
        {
            SteamAudioManager.Initialize(ManagerInitReason.Baking);

            if (SteamAudioManager.GetSceneType() == SceneType.Custom)
            {
                Debug.LogError("Baking is not supported when using Unity's built-in ray tracer. Click Steam Audio > Settings and switch to a different ray tracer.");
                return;
            }

            SteamAudioManager.LoadScene(SceneManager.GetActiveScene(), SteamAudioManager.Context, false);

            SteamAudioStaticMesh staticMeshComponent = null;
            var rootObjects = SceneManager.GetActiveScene().GetRootGameObjects();
            foreach (var rootObject in rootObjects)
            {
                staticMeshComponent = rootObject.GetComponentInChildren<SteamAudioStaticMesh>();
                if (staticMeshComponent)
                    break;
            }

            if (staticMeshComponent == null || staticMeshComponent.asset == null)
            {
                Debug.LogError(string.Format("Scene {0} has not been exported. Click Steam Audio > Export Active Scene to do so.", SceneManager.GetActiveScene().name));
                return;
            }

            var staticMesh = new StaticMesh(SteamAudioManager.Context, SteamAudioManager.CurrentScene, staticMeshComponent.asset);
            staticMesh.AddToScene(SteamAudioManager.CurrentScene);

            SteamAudioManager.CurrentScene.Commit();

            staticMesh.Release();

            sTasks = tasks;
            sStatus = BakeStatus.InProgress;

            sProgressCallback = new ProgressCallback(AdvanceProgress);

#if (UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN)
            sProgressCallbackPointer = Marshal.GetFunctionPointerForDelegate(sProgressCallback);
            sProgressCallbackHandle = GCHandle.Alloc(sProgressCallbackPointer);
            GC.Collect();
#endif

#if UNITY_EDITOR
            EditorApplication.update += InEditorUpdate;
#endif

#if UNITY_EDITOR
            sShowModalProgressBar = showModalProgressBar;
            sProgressId = Progress.Start("Baking");
#endif

            sThread = new Thread(BakeThread);
            sThread.Start();
        }

        public static void EndBake()
        {
            if (sThread != null)
            {
                sThread.Join();
            }

            SerializedObject.FlushAllWrites();
            SteamAudioReverbDataPoint.FlushAllWrites();
#if UNITY_EDITOR
            UnityEditor.AssetDatabase.SaveAssets();
            UnityEditor.AssetDatabase.Refresh();
#endif

#if (UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN)
            if (sProgressCallbackHandle.IsAllocated)
            {
                sProgressCallbackHandle.Free();
            }
#endif

            SteamAudioManager.ShutDown();
            UnityEngine.Object.DestroyImmediate(SteamAudioManager.Singleton.gameObject);

#if UNITY_EDITOR
            sProgress = 0.0f;
#endif
            sNumSubTasks = 0;
            sNumSubTasksCompleted = 0;

            sStatus = BakeStatus.Ready;

#if UNITY_EDITOR
            EditorApplication.update -= InEditorUpdate;
#endif

#if UNITY_EDITOR
            Progress.Remove(sProgressId);
            sShowModalProgressBar = false;
#endif
        }

        public static bool IsBakeActive()
        {
            return (sStatus != BakeStatus.Ready);
        }

        public static bool DrawProgressBar()
        {
#if UNITY_EDITOR
            if (sStatus != BakeStatus.InProgress)
                return false;

            var progress = Progress.GetProgress(sProgressId);
            var description = Progress.GetDescription(sProgressId);
            EditorGUI.ProgressBar(EditorGUILayout.GetControlRect(), progress, description);
            if (GUILayout.Button("Cancel"))
            {
                CancelBake();
                return false;
            }
#endif
            return true;
        }

        static void UpdateBakeProgress(int taskIndex, int numTasks, string taskName, int subTaskIndex = 0, int numSubTasks = 1, string subTaskName = null)
        {
#if UNITY_EDITOR
            var progress = ((sNumSubTasksCompleted + sProgress) / Mathf.Max(sNumSubTasks, 1)) + .01f; // Adding an offset because progress bar when it is exact 0 has some non-zero progress.

            var progressString = string.Format("Task {0} / {1} [{2}]", taskIndex + 1, numTasks, taskName);
            if (subTaskName != null)
            {
                progressString += string.Format(", Probe Batch {0} / {1} [{2}]", subTaskIndex + 1, numSubTasks, subTaskName);
            }

            var progressPercent = Mathf.FloorToInt(Mathf.Min(progress * 100.0f, 100.0f));
            progressString += string.Format(" ({0}% complete)", progressPercent);

            Progress.Report(sProgressId, progress, progressString);
#endif
        }

        public static void CancelBake()
        {
            // Ensures partial baked data is not serialized and that bake is properly canceled for multiple
            // probe boxes.
            sCancel = true;
            API.iplReflectionsBakerCancelBake(SteamAudioManager.Context.Get());
#if UNITY_EDITOR
            EditorUtility.ClearProgressBar();
#endif
            EndBake();
            sCancel = false;
        }

        [MonoPInvokeCallback(typeof(ProgressCallback))]
        static void AdvanceProgress(float progress, IntPtr userData)
        {
#if UNITY_EDITOR
            sProgress = progress;
#endif
        }

        static void InEditorUpdate()
        {
#if UNITY_EDITOR
            if (sShowModalProgressBar && sProgressId >= 0)
            {
                var progress = Progress.GetProgress(sProgressId);
                var description = Progress.GetDescription(sProgressId);
                if (EditorUtility.DisplayCancelableProgressBar("Baking", description, progress))
                {
                    CancelBake();
                    return;
                }
            }

            if (sStatus == BakeStatus.Complete)
            {
                EditorUtility.ClearProgressBar();
                EndBake();
                EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
            }
#endif
        }

        static void BakeThread()
        {
            sNumSubTasks = 0;
            sNumSubTasksCompleted = 0;
            for (var i = 0; i < sTasks.Length; i++)
            {
                if (sTasks[i].probeBatches != null)
                {
                    sNumSubTasks += sTasks[i].probeBatches.Length;
                }
                else
                {
                    sNumSubTasks++;
                }
            }

            for (var i = 0; i < sTasks.Length; ++i)
            {
                var taskName = "";
                if (sTasks[i].identifier.type == BakedDataType.Pathing)
                {
                    taskName = string.Format("{0} (Pathing)", sTasks[i].name);
                }
                else if (sTasks[i].identifier.variation == BakedDataVariation.Reverb)
                {
                    taskName = string.Format("{0} (Reverb)", sTasks[i].name);
                }
                else
                {
                    taskName = string.Format("{0} (Reflections)", sTasks[i].name);
                }

                Debug.Log(string.Format("START: Baking effect for {0}.", taskName));

                if (sTasks[i].probeBatches != null)
                {
                    var probeBatches = sTasks[i].probeBatches;

                    for (var j = 0; j < probeBatches.Length; ++j)
                    {
                        if (sCancel)
                            return;

                        if (probeBatches[j] == null)
                        {
                            Debug.LogWarning(string.Format("{0}: Probe Batch at index {1} is null, skipping.", taskName, j));
                            continue;
                        }

                        if (probeBatches[j].GetNumProbes() == 0)
                        {
                            Debug.LogWarning(string.Format("{0}: Probe Batch {1} has no probes, skipping.", taskName, sTasks[i].probeBatchNames[j]));
                            continue;
                        }

                        var probeBatch = new ProbeBatch(SteamAudioManager.Context, sTasks[i].probeBatchAssets[j]);

                        var simulationSettings = SteamAudioManager.GetSimulationSettings(true);

                        if (sTasks[i].identifier.type == BakedDataType.Reflections)
                        {
                            var bakeParams = new ReflectionsBakeParams { };
                            bakeParams.scene = SteamAudioManager.CurrentScene.Get();
                            bakeParams.probeBatch = probeBatch.Get();
                            bakeParams.sceneType = simulationSettings.sceneType;
                            bakeParams.identifier = sTasks[i].identifier;
                            bakeParams.flags = 0;
                            bakeParams.numRays = simulationSettings.maxNumRays;
                            bakeParams.numDiffuseSamples = simulationSettings.numDiffuseSamples;
                            bakeParams.numBounces = SteamAudioSettings.Singleton.bakingBounces;
                            bakeParams.simulatedDuration = simulationSettings.maxDuration;
                            bakeParams.savedDuration = simulationSettings.maxDuration;
                            bakeParams.order = simulationSettings.maxOrder;
                            bakeParams.numThreads = simulationSettings.numThreads;
                            bakeParams.rayBatchSize = simulationSettings.rayBatchSize;
                            bakeParams.irradianceMinDistance = SteamAudioSettings.Singleton.bakingIrradianceMinDistance;
                            bakeParams.bakeBatchSize = 1;

                            if (SteamAudioSettings.Singleton.bakeConvolution)
                                bakeParams.flags = bakeParams.flags | ReflectionsBakeFlags.BakeConvolution;

                            if (SteamAudioSettings.Singleton.bakeParametric)
                                bakeParams.flags = bakeParams.flags | ReflectionsBakeFlags.BakeParametric;

                            if (simulationSettings.sceneType == SceneType.RadeonRays)
                            {
                                bakeParams.openCLDevice = SteamAudioManager.OpenCLDevice;
                                bakeParams.radeonRaysDevice = SteamAudioManager.RadeonRaysDevice;
                                bakeParams.bakeBatchSize = SteamAudioSettings.Singleton.bakingBatchSize;
                            }

                            API.iplReflectionsBakerBake(SteamAudioManager.Context.Get(), ref bakeParams, sProgressCallback, IntPtr.Zero);
                        }
                        else
                        {
                            var bakeParams = new PathBakeParams { };
                            bakeParams.scene = SteamAudioManager.CurrentScene.Get();
                            bakeParams.probeBatch = probeBatch.Get();
                            bakeParams.identifier = sTasks[i].identifier;
                            bakeParams.numSamples = SteamAudioSettings.Singleton.bakingVisibilitySamples;
                            bakeParams.radius = SteamAudioSettings.Singleton.bakingVisibilityRadius;
                            bakeParams.threshold = SteamAudioSettings.Singleton.bakingVisibilityThreshold;
                            bakeParams.visRange = SteamAudioSettings.Singleton.bakingVisibilityRange;
                            bakeParams.pathRange = SteamAudioSettings.Singleton.bakingPathRange;
                            bakeParams.numThreads = SteamAudioManager.Singleton.NumThreadsForCPUCorePercentage(SteamAudioSettings.Singleton.bakedPathingCPUCoresPercentage);

                            API.iplPathBakerBake(SteamAudioManager.Context.Get(), ref bakeParams, sProgressCallback, IntPtr.Zero);
                        }

                        if (sCancel)
                        {
                            Debug.Log("CANCELLED: Baking.");
                            return;
                        }

                        // Don't flush the writes to disk just yet, because we can only do it from the main thread.
                        probeBatches[j].probeDataSize = probeBatch.Save(sTasks[i].probeBatchAssets[j], false);

                        var dataSize = (int)probeBatch.GetDataSize(sTasks[i].identifier);
                        probeBatches[j].AddOrUpdateLayer(sTasks[i].gameObject, sTasks[i].identifier, dataSize);

                        if (sTasks[i].identifier.type == BakedDataType.Reflections)
                        {
                            switch (sTasks[i].identifier.variation)
                            {
                                case BakedDataVariation.Reverb:
                                    (sTasks[i].component as SteamAudioListener).UpdateBakedDataStatistics();
                                    break;

                                case BakedDataVariation.StaticSource:
                                    (sTasks[i].component as SteamAudioBakedSource).UpdateBakedDataStatistics();
                                    break;

                                case BakedDataVariation.StaticListener:
                                    (sTasks[i].component as SteamAudioBakedListener).UpdateBakedDataStatistics();
                                    break;
                            }
                        }

                        sNumSubTasksCompleted++;
                        UpdateBakeProgress(i, sTasks.Length, taskName, j, sTasks[i].probeBatches.Length, sTasks[i].probeBatchNames[j]);
                    }
                }

                if (sTasks[i].probe != null)
                { 
                    var probe = sTasks[i].probe;
                    var probePosition = sTasks[i].probePosition;

                    if (sCancel)
                        return;

                    var probeBatch = new ProbeBatch(SteamAudioManager.Context);
                    SteamAudio.Sphere sphere = new SteamAudio.Sphere { center = Common.ConvertVector(probePosition), radius = 10.0f };
                    probeBatch.AddProbe(sphere);
                    probeBatch.Commit();

                    var simulationSettings = SteamAudioManager.GetSimulationSettings(true);
                    simulationSettings.maxDuration = probe.reverbDuration;
                    simulationSettings.maxOrder = probe.ambisonicOrder;

                    if (sTasks[i].identifier.type == BakedDataType.Reflections)
                    {
                        var bakeParams = new ReflectionsBakeParams { };
                        bakeParams.scene = SteamAudioManager.CurrentScene.Get();
                        bakeParams.probeBatch = probeBatch.Get();
                        bakeParams.sceneType = simulationSettings.sceneType;
                        bakeParams.identifier = sTasks[i].identifier;
                        bakeParams.flags = 0;
                        bakeParams.numRays = simulationSettings.maxNumRays;
                        bakeParams.numDiffuseSamples = simulationSettings.numDiffuseSamples;
                        bakeParams.numBounces = SteamAudioSettings.Singleton.bakingBounces;
                        bakeParams.simulatedDuration = simulationSettings.maxDuration;
                        bakeParams.savedDuration = simulationSettings.maxDuration;
                        bakeParams.order = simulationSettings.maxOrder;
                        bakeParams.numThreads = simulationSettings.numThreads;
                        bakeParams.rayBatchSize = simulationSettings.rayBatchSize;
                        bakeParams.irradianceMinDistance = SteamAudioSettings.Singleton.bakingIrradianceMinDistance;
                        bakeParams.bakeBatchSize = 1;

                        bakeParams.flags = bakeParams.flags | ReflectionsBakeFlags.BakeConvolution;
                        bakeParams.flags = bakeParams.flags | ReflectionsBakeFlags.BakeParametric;

                        if (simulationSettings.sceneType == SceneType.RadeonRays)
                        {
                            bakeParams.openCLDevice = SteamAudioManager.OpenCLDevice;
                            bakeParams.radeonRaysDevice = SteamAudioManager.RadeonRaysDevice;
                            bakeParams.bakeBatchSize = SteamAudioSettings.Singleton.bakingBatchSize;
                        }

                        API.iplReflectionsBakerBake(SteamAudioManager.Context.Get(), ref bakeParams, sProgressCallback, IntPtr.Zero);

                        if (probe.reverbData.reverbTimes.Length != 3)
                        {
                            probe.reverbData.reverbTimes = new float[3];
                            probe.reverbData.reverbTimes[0] = .0f;
                            probe.reverbData.reverbTimes[1] = .0f;
                            probe.reverbData.reverbTimes[2] = .0f;
                        }

                        // Get Energy Field and Impulse Response
                        {
                            // Energy Field
                            EnergyFieldSettings energyFieldSettings;
                            energyFieldSettings.duration = bakeParams.savedDuration;
                            energyFieldSettings.order = bakeParams.order;

                            IntPtr energyField;
                            API.iplEnergyFieldCreate(SteamAudioManager.Context.Get(), ref energyFieldSettings, out energyField);

                            API.iplProbeBatchGetEnergyField(probeBatch.Get(), ref sTasks[i].identifier, 0, energyField);
                            API.iplProbeBatchGetReverb(probeBatch.Get(), ref sTasks[i].identifier, 0, probe.reverbData.reverbTimes);
                            probe.UpdateEnergyField(energyField);

                            // Create Reconstructor
                            ReconstructorSettings reconstructorSettings;
                            reconstructorSettings.maxDuration = bakeParams.savedDuration;
                            reconstructorSettings.maxOrder = bakeParams.order;
                            reconstructorSettings.samplingRate = probe.sampleRate;

                            IntPtr irConstructor;
                            API.iplReconstructorCreate(SteamAudioManager.Context.Get(), ref reconstructorSettings, out irConstructor);

                            // Create Impulse Response
                            ImpulseResponseSettings irSettings;
                            irSettings.duration = bakeParams.savedDuration;
                            irSettings.order = bakeParams.order;
                            irSettings.samplingRate = probe.sampleRate;

                            IntPtr ir;
                            API.iplImpulseResponseCreate(SteamAudioManager.Context.Get(), ref irSettings, out ir);

                            // Reconstruct Impulse Response
                            ReconstructorInputs reconstructorInputs;
                            reconstructorInputs.energyField = energyField;

                            ReconstructorSharedInputs reconstructorSharedInputs;
                            reconstructorSharedInputs.duration = bakeParams.savedDuration;
                            reconstructorSharedInputs.order = bakeParams.order;

                            ReconstructorOutputs reconstructorOutputs;
                            reconstructorOutputs.impulseResponse = ir;

                            API.iplReconstructorReconstruct(irConstructor, 1, ref reconstructorInputs, ref reconstructorSharedInputs, ref reconstructorOutputs);
                            probe.UpdateImpulseResponse(ir);

                            API.iplImpulseResponseRelease(ref ir);
                            API.iplReconstructorRelease(ref irConstructor);
                            API.iplEnergyFieldRelease(ref energyField);
                            probe.WriteReverbDataToFile(flush: false);
                        }

                        probeBatch.Release();
                    }

                    if (sCancel)
                    {
                        Debug.Log("CANCELLED: Baking.");
                        return;
                    }

                    sNumSubTasksCompleted++;
                    UpdateBakeProgress(i, sTasks.Length, taskName);
                }

                Debug.Log(string.Format("COMPLETED: Baking effect for {0}.", taskName));
            }

            sStatus = BakeStatus.Complete;
        }
    }
}

#endif
