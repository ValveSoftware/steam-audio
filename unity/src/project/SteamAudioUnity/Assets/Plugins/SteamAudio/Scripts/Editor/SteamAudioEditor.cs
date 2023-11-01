//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System.Linq;
using UnityEditor;
using UnityEditor.Build;
using UnityEngine;

namespace SteamAudio
{
    public abstract class SteamAudioEditor : Editor
    {
        private static class Styles
        {
            public static readonly GUIContent sDisableButton = new GUIContent("Disable Steam Audio", $"Click this button to automatically add the STEAMAUDIO_DISABLED define symbol for this target");
            public static readonly GUIContent sEnableButton = new GUIContent("Enable Steam Audio", "Click this button to automatically remove the STEAMAUDIO_DISABLED define symbol for this target");
        }
        
        static bool? sNativePluginAvailableForTarget = null;
        public static bool NativePluginAvailableForTarget
        {
            get
            {
                if (sNativePluginAvailableForTarget != null)
                    return sNativePluginAvailableForTarget.Value;

                sNativePluginAvailableForTarget = AssetDatabase.FindAssets("phonon t:DefaultAsset")
                    .Select(guid => AssetImporter.GetAtPath(AssetDatabase.GUIDToAssetPath(guid)) as PluginImporter)
                    .Where(plugin => plugin != null)
                    .Any(IsNativePluginCompatibleWithActiveTarget);

                return sNativePluginAvailableForTarget.Value;
            }
        }

        private static bool IsNativePluginCompatibleWithActiveTarget(PluginImporter plugin)
        {
            // A standalone plugin will be considered as compatible to all standalone targets.
            // Since Steam Audio provides binary for all standalone platform, telling them apart is not a concern.
            // https://github.com/Unity-Technologies/UnityCsReference/blob/2021.3/Modules/AssetPipelineEditor/ImportSettings/PluginImporterInspector.cs#L28

            if (!plugin.isNativePlugin) return false;
            if (!plugin.GetCompatibleWithPlatform(EditorUserBuildSettings.activeBuildTarget)) return false;
            return true;
        }

#if UNITY_2021_2_OR_NEWER
        public static NamedBuildTarget GetActiveNamedBuildTarget()
        {
#if UNITY_SERVER
            return NamedBuildTarget.Server;
#endif
            var targetGroup = BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget);
            return NamedBuildTarget.FromBuildTargetGroup(targetGroup);
        }

        public static void AddDefineSymbolsToTarget(string define)
        {
            var target = GetActiveNamedBuildTarget();
            var defines = PlayerSettings.GetScriptingDefineSymbols(target);
            var defineList = defines.Split(';').ToList();
            if (defineList.IndexOf(define) < 0)
            {
                PlayerSettings.SetScriptingDefineSymbols(target, defines + ";" + define);
            }
        }

        public static void RemoveDefineSymbolsToTarget(string define)
        {
            var target = GetActiveNamedBuildTarget();
            var defines = PlayerSettings.GetScriptingDefineSymbols(target).Split(';').ToList();
            defines.Remove(define);
            PlayerSettings.SetScriptingDefineSymbols(target, string.Join(';', defines));
        }
#else
        public static void AddDefineSymbolsToTarget(string define)
        {
            var target = BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget);
            var defines = PlayerSettings.GetScriptingDefineSymbolsForGroup(target).Split(';').ToList();
            if (defines.IndexOf(define) < 0)
            {
                PlayerSettings.SetScriptingDefineSymbolsForGroup(target, defines + ";" + define);
            }
        }

        public static void RemoveDefineSymbolsToTarget(string define)
        {
            var target = BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget);
            var defines = PlayerSettings.GetScriptingDefineSymbolsForGroup(target).Split(';').ToList();
            defines.Remove(define);
            PlayerSettings.SetScriptingDefineSymbolsForGroup(target, string.Join(';', defines));
        }
#endif

        protected virtual bool AlwaysShowSteamAudioToggle { get; set; } = false;

        public override void OnInspectorGUI()
        {
            if (EditorApplication.isCompiling) return;

#if STEAMAUDIO_DISABLED
            if (NativePluginAvailableForTarget)
            {
                EditorGUILayout.HelpBox("Steam Audio is disabled for {EditorUserBuildSettings.activeBuildTarget}.\nSteam Audio native plugin found for {EditorUserBuildSettings.activeBuildTarget}.\nYou can enable Steam Audio by removing the STEAMAUDIO_DISABLED define symbol.", MessageType.Info);
                if (GUILayout.Button(Styles.sEnableButton))
                {
                    RemoveDefineSymbolsToTarget("STEAMAUDIO_DISABLED");
                }
            }
            else
            {
                EditorGUILayout.HelpBox("Steam Audio is disabled for {EditorUserBuildSettings.activeBuildTarget}.", AlwaysShowSteamAudioToggle ? MessageType.Info : MessageType.None);
                if (AlwaysShowSteamAudioToggle && GUILayout.Button(Styles.sEnableButton))
                {
                    RemoveDefineSymbolsToTarget("STEAMAUDIO_DISABLED");
                }
            }
#else
            if (!NativePluginAvailableForTarget)
            {
                EditorGUILayout.HelpBox($"Steam Audio native plugin is not available for {EditorUserBuildSettings.activeBuildTarget}.\nYou can disable Steam Audio by adding a STEAMAUDIO_DISABLED define symbol to prevent Steam Audio from loading on this platform.", MessageType.Warning);
                if (GUILayout.Button(Styles.sDisableButton))
                {
                    AddDefineSymbolsToTarget("STEAMAUDIO_DISABLED");
                }
            }
            else if (AlwaysShowSteamAudioToggle)
            {
                // In case user want to disable Steam Audio on supported platform,
                // this disable button will only show up in Steam Audio Settings.
                EditorGUILayout.HelpBox($"Steam Audio is enabled for {EditorUserBuildSettings.activeBuildTarget}.", MessageType.Info);
                if (GUILayout.Button(Styles.sDisableButton))
                {
                    AddDefineSymbolsToTarget("STEAMAUDIO_DISABLED");
                }
            }
#endif
            OnSteamAudioGUI();
        }

        protected virtual void OnSteamAudioGUI() {}
    }
}
