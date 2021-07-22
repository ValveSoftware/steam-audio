//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace SteamAudio
{
    public class SerializedData : ScriptableObject
    {
        [SerializeField]
        public byte[] data;

        public static SerializedData PromptForNewAsset(string defaultFileName)
        {
#if UNITY_EDITOR
            var fileName = EditorUtility.SaveFilePanelInProject("Export", defaultFileName, "asset",
                "Select a file to export data to.");

            if (fileName != null && fileName.Length > 0)
            {
                var dataAsset = ScriptableObject.CreateInstance<SerializedData>();
                AssetDatabase.CreateAsset(dataAsset, fileName);
                return dataAsset;
            }
#endif
            return null;
        }
    }
}
