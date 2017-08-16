//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;

namespace SteamAudio
{
    public class SteamAudioWindow : EditorWindow
    {

        [MenuItem("Window/Steam Audio")]
        static void Init()
        {
#pragma warning disable 618
            SteamAudioWindow window = GetWindow<SteamAudioWindow>();
            window.title = "Steam Audio";
            window.Show();
#pragma warning restore 618
        }
        
        void OnEnable()
        {
            autoRepaintOnSceneChange = true;
        }

        void OnInspectorUpdate()
        {
            Repaint();
        }
        
        void OnGUI()
        {
            if (phononManager == null || editor == null)
            {
                string name = "Steam Audio Manager Settings";
                GameObject managerObject = GameObject.Find(name);
                if (managerObject == null)
                {
                    targetObject = new GameObject(name);
                    phononManager = targetObject.AddComponent<SteamAudioManager>();
                    editor = Editor.CreateEditor(phononManager);
                    EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
                }
                else
                {
                    phononManager = managerObject.GetComponent<SteamAudioManager>();
                    editor = Editor.CreateEditor(phononManager);
                }
            }

            editor.OnInspectorGUI();
        }

        SteamAudioManager phononManager = null;
        GameObject targetObject = null;
        Editor editor = null;
    }
}