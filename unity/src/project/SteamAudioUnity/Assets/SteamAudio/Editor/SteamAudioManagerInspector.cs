//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;
using UnityEngine.SceneManagement;
using UnityEditor.SceneManagement;

using System;
using System.Collections.Generic;

namespace SteamAudio
{

    //
    // SteamAudioManagerInspector
    // Custom inspector for a SteamAudioManager component.
    //

    [CustomEditor(typeof(SteamAudioManager))]
    public class SteamAudioManagerInspector : Editor
    {
        //
        // Draws the inspector.
        //
        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            // Audio Engine
            GUI.enabled = !EditorApplication.isPlayingOrWillChangePlaymode;
            EditorGUILayout.Space();
            EditorGUILayout.HelpBox("Do not manually add Steam Audio Manager component. " +
                "Click Window > Steam Audio.", MessageType.Info);
            EditorGUILayout.LabelField("Audio Engine Integration", EditorStyles.boldLabel);
            string[] engines = { "Unity", "FMOD Studio" };
            var audioEngineProperty = serializedObject.FindProperty("audioEngine");
            audioEngineProperty.enumValueIndex = EditorGUILayout.Popup("Audio Engine",
                audioEngineProperty.enumValueIndex, engines);

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("HRTF Settings", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(serializedObject.FindProperty("sofaFiles"), new GUIContent("SOFA Files"), true);

            SteamAudioManager steamAudioManager = (SteamAudioManager)target;

            var wasGUIEnabled = GUI.enabled;
            GUI.enabled = true;

            var sofaFiles = steamAudioManager.GetSOFAFileNames();
            if (sofaFiles != null) {
                var sofaFileNames = new string[sofaFiles.Length];
                for (var i = 0; i < sofaFileNames.Length; ++i) {
                    sofaFileNames[i] = sofaFiles[i];
                    if (sofaFileNames[i].Length == 0 || sofaFileNames[i] == "") {
                        sofaFileNames[i] = "Default";
                    }
                }
                var currentSOFAFileProperty = serializedObject.FindProperty("currentSOFAFile");
                if (currentSOFAFileProperty.intValue >= sofaFiles.Length) 
                {
                    currentSOFAFileProperty.intValue = 0;
                }
                currentSOFAFileProperty.intValue = EditorGUILayout.Popup("Current SOFA File",
                    currentSOFAFileProperty.intValue, sofaFileNames);
            }

            GUI.enabled = wasGUIEnabled;

            // Scene Settings
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Global Material Settings", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(serializedObject.FindProperty("materialPreset"));
            if (serializedObject.FindProperty("materialPreset").enumValueIndex < 11)
            {
                MaterialValue actualValue = steamAudioManager.materialValue;
                actualValue.CopyFrom(MaterialPresetList.PresetValue(
                    serializedObject.FindProperty("materialPreset").enumValueIndex));
            }
            else
            {
                EditorGUILayout.PropertyField(serializedObject.FindProperty("materialValue"));
            }

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Scene Export", EditorStyles.boldLabel);
            if (GUILayout.Button("Pre-Export Scene"))
                steamAudioManager.ExportScene(false);
            if (GUILayout.Button("Export All Dynamic Objects")) {
                ExportAllDynamicObjects();
            }
            if (GUILayout.Button("Export to OBJ"))
                steamAudioManager.ExportScene(true);

            // Simulation Settings
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Simulation Settings", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(serializedObject.FindProperty("simulationPreset"));
            if (serializedObject.FindProperty("simulationPreset").enumValueIndex < 3)
            {
                SimulationSettingsValue actualValue = steamAudioManager.simulationValue;
                actualValue.CopyFrom(SimulationSettingsPresetList.PresetValue(
                    serializedObject.FindProperty("simulationPreset").enumValueIndex));

                EditorGUILayout.Space();
                UnityRayTracerGUI(true);
            } else
            {
                EditorGUILayout.PropertyField(serializedObject.FindProperty("simulationValue"));

                SimulationSettingsValue actualValue = steamAudioManager.simulationValue;

                if (actualValue.AmbisonicsOrder > 2)
                {
                    var numChannels = (actualValue.AmbisonicsOrder + 1) * (actualValue.AmbisonicsOrder + 1);
                    EditorGUILayout.HelpBox("Ambisonics order " + actualValue.AmbisonicsOrder.ToString() +
                        " uses " + numChannels.ToString() + " channels per source for processing indirect sound. " +
                        "This may significantly increase CPU usage. Consider reducing this value unless necessary.",
                        MessageType.Warning);
                }

                UnityRayTracerGUI(false);

                if (Application.isEditor && EditorApplication.isPlayingOrWillChangePlaymode)
                {
                    IntPtr environment = steamAudioManager.GameEngineState().Environment().GetEnvironment();
                    if (environment != IntPtr.Zero)
                        PhononCore.iplSetNumBounces(environment, actualValue.RealtimeBounces);
                }
            }

            // Display information message about CPU usage.
            int realTimeThreads = (int)Mathf.Max(1, (steamAudioManager.simulationValue.RealtimeThreadsPercentage * SystemInfo.processorCount) / 100.0f);
            int bakingThreads = (int)Mathf.Max(1, (steamAudioManager.simulationValue.BakeThreadsPercentage * SystemInfo.processorCount) / 100.0f);
            string realTimeString = (realTimeThreads == 1) ? "1 of " + SystemInfo.processorCount + " logical processor" : 
                                                            realTimeThreads + " of " + SystemInfo.processorCount + " logical processors";
            string bakingString = (bakingThreads == 1) ? "1 of " + SystemInfo.processorCount + " logical processor" :
                                                            bakingThreads + " of " + SystemInfo.processorCount + " logical processors";
            EditorGUILayout.HelpBox("On this machine setting realtime CPU cores to " + steamAudioManager.simulationValue.RealtimeThreadsPercentage + 
                "% will use " + realTimeString + " and setting baking CPU cores to " + steamAudioManager.simulationValue.BakeThreadsPercentage +
                "% will use " + bakingString + ". The number of logical processors used on an end user's machine might be different.", MessageType.Info);

            // Fold Out for Advanced Settings
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Advanced Options", EditorStyles.boldLabel);
            serializedObject.FindProperty("showLoadTimeOptions").boolValue = 
            EditorGUILayout.Foldout(serializedObject.FindProperty("showLoadTimeOptions").boolValue, 
                "Per Frame Query Optimization");

            if (steamAudioManager.showLoadTimeOptions)
            {
                EditorGUILayout.PropertyField(serializedObject.FindProperty("updateComponents"));

                var updateComponents = serializedObject.FindProperty("updateComponents").boolValue;
                if (!updateComponents)
                    EditorGUILayout.PropertyField(serializedObject.FindProperty("currentAudioListener"));
            }

            serializedObject.FindProperty("showMassBakingOptions").boolValue = 
                EditorGUILayout.Foldout(serializedObject.FindProperty("showMassBakingOptions").boolValue, 
                "Consolidated Baking Options");
            if (steamAudioManager.showMassBakingOptions)
            {
                bool noSettingMessage = false;
                noSettingMessage = ProbeGenerationGUI() || noSettingMessage;
                noSettingMessage = BakeAllGUI() || noSettingMessage;
                noSettingMessage = BakedSourcesGUI(steamAudioManager) || noSettingMessage;
                noSettingMessage =  BakedStaticListenerNodeGUI(steamAudioManager) || noSettingMessage;
                noSettingMessage = BakedReverbGUI(steamAudioManager) || noSettingMessage;

                if (!noSettingMessage)
                    EditorGUILayout.LabelField("Scene does not contain any baking related components.");
            }

            GUI.enabled = true;
            EditorGUILayout.Space();
            serializedObject.ApplyModifiedProperties();
        }

        void UnityRayTracerGUI(bool showHeader)
        {
            var customSettings = GameObject.FindObjectOfType<SteamAudioCustomSettings>();
            if (customSettings && customSettings.rayTracerOption == SceneType.Custom) {
                if (showHeader) {
                    EditorGUILayout.LabelField("Occlusion Settings", EditorStyles.boldLabel);
                }
                EditorGUILayout.PropertyField(serializedObject.FindProperty("layerMask"));
            }
        }

        public bool ProbeGenerationGUI()
        {
            SteamAudioProbeBox[] probeBoxes = GameObject.FindObjectsOfType<SteamAudioProbeBox>();
            if (probeBoxes.Length > 0)
                EditorGUILayout.LabelField("Probe Generation", EditorStyles.boldLabel);
            else
                return false;

            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            foreach (SteamAudioProbeBox probeBox in probeBoxes)
            {
                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(probeBox.name);
                if (GUILayout.Button("Generate Probe", GUILayout.Width(200.0f)))
                {
                    probeBox.GenerateProbes();
                    EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
                }
                EditorGUILayout.EndHorizontal();
            }
            GUI.enabled = true;

            return true;
        }

        public bool BakeAllGUI()
        {
            bool hasBakeComponents = GameObject.FindObjectsOfType<SteamAudioProbeBox>().Length > 0;

            if (hasBakeComponents)
                EditorGUILayout.LabelField("Bake All", EditorStyles.boldLabel);
            else
                return false;

            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            EditorGUILayout.BeginHorizontal();

            if (GUILayout.Button("Select All"))
            {
                SelectForBakeEffect(true);
            }

            if (GUILayout.Button("Select None"))
            {
                SelectForBakeEffect(false);
            }

            if (GUILayout.Button("Bake", GUILayout.Width(200.0f)))
            {
                BakeSelected();
            }

            EditorGUILayout.EndHorizontal();
            GUI.enabled = true;

            DisplayProgressBarAndCancel();

            return true;
        }

        void DisplayProgressBarAndCancel()
        {
            SteamAudioManager phononManager = serializedObject.targetObject as SteamAudioManager;
            phononManager.phononBaker.DrawProgressBar();
            Repaint();
        }

        public void BakeSelected()
        {
            List<GameObject> gameObjects = new List<GameObject>();
            List<BakedDataIdentifier> identifers = new List<BakedDataIdentifier>();
            List<string> names = new List<string>();
            List<Sphere> influenceSpheres = new List<Sphere>();
            List<SteamAudioProbeBox[]> probeBoxes = new List<SteamAudioProbeBox[]>();

            SteamAudioSource[] bakedSources = GameObject.FindObjectsOfType<SteamAudioSource>();
            foreach (SteamAudioSource bakedSource in bakedSources)
            {
                if (bakedSource.uniqueIdentifier.Length != 0 && bakedSource.bakeToggle)
                {
                    gameObjects.Add(bakedSource.gameObject);
                    identifers.Add(bakedSource.bakedDataIdentifier);
                    names.Add(bakedSource.uniqueIdentifier);

                    Sphere bakeSphere;
                    Vector3 sphereCenter = Common.ConvertVector(bakedSource.transform.position);
                    bakeSphere.centerx = sphereCenter.x;
                    bakeSphere.centery = sphereCenter.y;
                    bakeSphere.centerz = sphereCenter.z;
                    bakeSphere.radius = bakedSource.bakingRadius;
                    influenceSpheres.Add(bakeSphere);

                    if (bakedSource.useAllProbeBoxes)
                        probeBoxes.Add(FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[]);
                    else
                        probeBoxes.Add(bakedSource.probeBoxes);
                }
            }

            SteamAudioBakedStaticListenerNode[] bakedStaticNodes = GameObject.FindObjectsOfType<SteamAudioBakedStaticListenerNode>();
            foreach (SteamAudioBakedStaticListenerNode bakedStaticNode in bakedStaticNodes)
            {
                if (bakedStaticNode.uniqueIdentifier.Length != 0 && bakedStaticNode.bakeToggle)
                {
                    gameObjects.Add(bakedStaticNode.gameObject);
                    identifers.Add(bakedStaticNode.bakedDataIdentifier);
                    names.Add(bakedStaticNode.name);

                    Sphere bakeSphere;
                    Vector3 sphereCenter = Common.ConvertVector(bakedStaticNode.transform.position);
                    bakeSphere.centerx = sphereCenter.x;
                    bakeSphere.centery = sphereCenter.y;
                    bakeSphere.centerz = sphereCenter.z;
                    bakeSphere.radius = bakedStaticNode.bakingRadius;
                    influenceSpheres.Add(bakeSphere);

                    if (bakedStaticNode.useAllProbeBoxes)
                        probeBoxes.Add(FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[]);
                    else
                        probeBoxes.Add(bakedStaticNode.probeBoxes);
                }
            }

            SteamAudioListener bakedReverb = GameObject.FindObjectOfType<SteamAudioListener>();
            if (!(bakedReverb == null) && bakedReverb.bakeToggle)
            {
                gameObjects.Add(bakedReverb.gameObject);
                identifers.Add(bakedReverb.Identifier);
                names.Add("reverb");
                influenceSpheres.Add(new Sphere());

                if (bakedReverb.useAllProbeBoxes)
                    probeBoxes.Add(FindObjectsOfType<SteamAudioProbeBox>() as SteamAudioProbeBox[]);
                else
                    probeBoxes.Add(bakedReverb.probeBoxes);
            }

            if (gameObjects.Count > 0)
            {
                SteamAudioManager phononManager = serializedObject.targetObject as SteamAudioManager;
                phononManager.phononBaker.BeginBake(gameObjects.ToArray(), identifers.ToArray(), names.ToArray(),
                    influenceSpheres.ToArray(), probeBoxes.ToArray());
            }
            else
            {
                Debug.LogWarning("No game object selected for baking.");
            }
        }

        public void SelectForBakeEffect(bool select)
        {
            SteamAudioSource[] bakedSources = GameObject.FindObjectsOfType<SteamAudioSource>();
            foreach (SteamAudioSource bakedSource in bakedSources)
            {
                if (bakedSource.uniqueIdentifier.Length != 0)
                {
                    bakedSource.bakeToggle = select;
                }
            }

            SteamAudioBakedStaticListenerNode[] bakedStaticNodes = GameObject.FindObjectsOfType<SteamAudioBakedStaticListenerNode>();
            foreach (SteamAudioBakedStaticListenerNode bakedStaticNode in bakedStaticNodes)
            {
                if (bakedStaticNode.uniqueIdentifier.Length != 0)
                {
                    bakedStaticNode.bakeToggle = select;
                }
            }

            SteamAudioListener bakedReverb = GameObject.FindObjectOfType<SteamAudioListener>();
            if (!(bakedReverb == null))
            {
                bakedReverb.bakeToggle = select;
            }

        }

        public bool BakedSourcesGUI(SteamAudioManager phononManager)
        {
            SteamAudioSource[] bakedSources = GameObject.FindObjectsOfType<SteamAudioSource>();

            bool showBakedSources = false;
            foreach (SteamAudioSource bakedSource in bakedSources)
            {
                if (bakedSource.uniqueIdentifier.Length != 0)
                {
                    showBakedSources = true;
                    break;
                }
            }

            if (showBakedSources)
                EditorGUILayout.LabelField("Baked Sources", EditorStyles.boldLabel);
            else
                return false;

            foreach (SteamAudioSource bakedSource in bakedSources)
            {
                if (bakedSource.uniqueIdentifier.Length == 0)
                    continue;

                GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
                EditorGUILayout.BeginHorizontal();

                bakedSource.UpdateBakedDataStatistics();
                bool previousValue = bakedSource.bakeToggle;
                bool newValue = GUILayout.Toggle(bakedSource.bakeToggle, " " + bakedSource.uniqueIdentifier);
                if (previousValue != newValue)
                {
                    Undo.RecordObject(bakedSource, "Toggled " + bakedSource.uniqueIdentifier +
                        " in Phonon Manager");
                    bakedSource.bakeToggle = newValue;
                }

                EditorGUILayout.LabelField((bakedSource.bakedDataSize / 1000.0f).ToString("0.0") + " KB");
                EditorGUILayout.EndHorizontal();
                GUI.enabled = true;
            }

            return true;
        }

        public bool BakedStaticListenerNodeGUI(SteamAudioManager phononManager)
        {
            SteamAudioBakedStaticListenerNode[] bakedStaticNodes = GameObject.FindObjectsOfType<SteamAudioBakedStaticListenerNode>();

            bool showBakedStaticListenerNodes = false;
            foreach (SteamAudioBakedStaticListenerNode bakedStaticNode in bakedStaticNodes)
                if (bakedStaticNode.uniqueIdentifier.Length != 0)
                {
                    showBakedStaticListenerNodes = true;
                    break;
                }

            if (showBakedStaticListenerNodes)
                EditorGUILayout.LabelField("Baked Static Listener Nodes", EditorStyles.boldLabel);
            else
                return false;

            foreach (SteamAudioBakedStaticListenerNode bakedStaticNode in bakedStaticNodes)
            {
                if (bakedStaticNode.uniqueIdentifier.Length == 0)
                    continue;

                GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
                EditorGUILayout.BeginHorizontal();
                bakedStaticNode.UpdateBakedDataStatistics();

                bool previousValue = bakedStaticNode.bakeToggle;
                bool newValue = GUILayout.Toggle(bakedStaticNode.bakeToggle, " " + 
                    bakedStaticNode.uniqueIdentifier);
                if (previousValue != newValue)
                {
                    Undo.RecordObject(bakedStaticNode, "Toggled " + bakedStaticNode.uniqueIdentifier +
                        " in Phonon Manager");
                    bakedStaticNode.bakeToggle = newValue;
                }
                EditorGUILayout.LabelField((bakedStaticNode.bakedDataSize / 1000.0f).ToString("0.0") + " KB");
                EditorGUILayout.EndHorizontal();
                GUI.enabled = true;
            }

            return true;
        }

        public bool BakedReverbGUI(SteamAudioManager phononManager)
        {
            SteamAudioListener bakedReverb = GameObject.FindObjectOfType<SteamAudioListener>();
            if (bakedReverb == null)
                return false;

            EditorGUILayout.LabelField("Baked Reverb", EditorStyles.boldLabel);

            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            EditorGUILayout.BeginHorizontal();
            bakedReverb.UpdateBakedDataStatistics();

            bool previousValues = bakedReverb.bakeToggle;
            bool newValue = GUILayout.Toggle(bakedReverb.bakeToggle, " reverb");
            if (previousValues != newValue)
            {
                Undo.RecordObject(bakedReverb, "Toggled reverb in Phonon Manager");
                bakedReverb.bakeToggle = newValue;
            }

            EditorGUILayout.LabelField((bakedReverb.bakedDataSize / 1000.0f).ToString("0.0") + " KB");
            EditorGUILayout.EndHorizontal();
            GUI.enabled = true;

            return true;
        }

        void ExportAllDynamicObjects()
        {
            Debug.Log("Starting batched export of Steam Audio Dynamic Objects...");

            var scenes = AssetDatabase.FindAssets("t:Scene");
            var prefabs = AssetDatabase.FindAssets("t:Prefab");

            int numItems = scenes.Length + prefabs.Length;

            // todo: what if we clicked export on the instance and not the prefab?
            Debug.Log("Steam Audio Dynamic Object: Searching scenes...");
            int index = 0;
            foreach (var sceneGUID in scenes) {
                var scenePath = AssetDatabase.GUIDToAssetPath(sceneGUID);
                Debug.Log(string.Format("Processing scene {0}...", scenePath));
                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Processing scene: {0}", scenePath), (float)index / (float)numItems);

                var activeScene = EditorSceneManager.GetActiveScene();
                var isLoadedScene = (scenePath == activeScene.path);

                var scene = activeScene;
                if (!isLoadedScene) {
                    scene = EditorSceneManager.OpenScene(scenePath, OpenSceneMode.Additive);
                }

                var rootObjects = scene.GetRootGameObjects();
                foreach (var rootObject in rootObjects) {
                    var dynamicObjects = rootObject.GetComponentsInChildren<SteamAudioDynamicObject>();
                    ExportDynamicObjectsInArray(false, scenePath, dynamicObjects);
                }

                if (!isLoadedScene) {
                    EditorSceneManager.CloseScene(scene, true);
                }

                ++index;
            }

            Debug.Log("Steam Audio Dynamic Object: Searching prefabs...");
            foreach (var prefabGUID in prefabs) {
                var prefabPath = AssetDatabase.GUIDToAssetPath(prefabGUID);
                Debug.Log(string.Format("Processing prefab {0}...", prefabPath));
                EditorUtility.DisplayProgressBar("Steam Audio", string.Format("Processing prefab: {0}", prefabPath), (float)index / (float)numItems);

                var prefab = AssetDatabase.LoadMainAssetAtPath(prefabPath) as GameObject;
                var dynamicObjects = prefab.GetComponentsInChildren<SteamAudioDynamicObject>();
                ExportDynamicObjectsInArray(true, prefabPath, dynamicObjects);

                ++index;
            }

            Debug.Log(string.Format("{0} scenes, {1} prefabs", scenes.Length, prefabs.Length));
            EditorUtility.DisplayProgressBar("Steam Audio", "", 1.0f);
            EditorUtility.ClearProgressBar();
        }

        void ExportDynamicObjectsInArray(bool isPrefab, string path, SteamAudioDynamicObject[] dynamicObjects)
        {
            var typeString = (isPrefab) ? "Prefab" : "Scene";

            foreach (var dynamicObject in dynamicObjects) {
                if (!isPrefab) {
                    var prefabType = PrefabUtility.GetPrefabType(dynamicObject);
                    if (prefabType == PrefabType.PrefabInstance) {
                        Debug.Log(string.Format("Scene {0}, GameObject {1}: Prefab instance detected, skipping " +
                            "export.", path, dynamicObject.name));
                        continue;
                    }
                }

                var hasAssetFileName = (dynamicObject.assetFileName != null &&
                    dynamicObject.assetFileName.Length > 0);
                if (hasAssetFileName) {
                    dynamicObject.Export(dynamicObject.assetFileName, false);
                } else {
                    Debug.LogWarning(string.Format("{0} {1}, GameObject {2}: Asset file name not set for " +
                        "Steam Audio Dynamic Object, skipping export.", typeString, path, dynamicObject.name));
                }
            }
        }
    }
}