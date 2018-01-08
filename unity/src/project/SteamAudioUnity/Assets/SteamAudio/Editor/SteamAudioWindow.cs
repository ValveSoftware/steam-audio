//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;
using System.Collections.Generic;

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
			if(renderHelper == null) {
				renderHelper = FindObjectOfType<OnRenderObjectHelper>();
				if(renderHelper == null) {
					GameObject go = new GameObject("RENDER_HELPER");
					go.hideFlags = HideFlags.HideAndDontSave;
					renderHelper = go.AddComponent<OnRenderObjectHelper>();
				}
			}
		}

		void OnDisable()
		{
			DestroyImmediate(renderHelper);
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

			if(renderHelper == null) {
				GUILayout.Label("Missing render helper");
				return;
			}

			if(GUILayout.Button("Scan for audio meshes")) {
				ScanForAudioMeshes();
			}
			GUILayout.BeginHorizontal();
			renderHelper.drawMeshes = GUILayout.Toggle(renderHelper.drawMeshes, "Draw meshes");
			renderHelper.showWireframe = GUILayout.Toggle(renderHelper.showWireframe, "Show wireframe");
			GUILayout.EndHorizontal();

			editor.OnInspectorGUI();
        }

		void ScanForAudioMeshes() {

			// Grab MeshFilters
			List<MeshFilter> filters = GetAllPhononGeometryMeshFilters();

			// Set up data for drawing meshes
			int count = filters.Count;
			renderHelper.meshes = new Mesh[count];
			renderHelper.matrices = new Matrix4x4[count];

			for(int i = 0; i < count; i++) {
				renderHelper.meshes[i] = filters[i].sharedMesh;
				renderHelper.matrices[i] = filters[i].transform.localToWorldMatrix;
			}

		}


		List<MeshFilter> GetAllPhononGeometryMeshFilters() {
			SteamAudio.SteamAudioGeometry[] audioGeo = FindObjectsOfType<SteamAudio.SteamAudioGeometry>();
			List<MeshFilter> filters = new List<MeshFilter>();

			for(int i = 0; i < audioGeo.Length; i++) {

				// Find all Mesh filters
				bool missingFilter = false;
				if(audioGeo[i].exportAllChildren) {
					MeshFilter[] filtersToAdd = audioGeo[i].GetComponentsInChildren<MeshFilter>();
					if(filtersToAdd.Length > 0) {
						filters.AddRange(filtersToAdd);
					}
					else {
						missingFilter = true;
					}
				}
				else {
					MeshFilter filterToAdd = audioGeo[i].GetComponent<MeshFilter>();
					if(filterToAdd != null) {
						filters.Add(filterToAdd);
					}
					else {
						missingFilter = true;
					}
				}
				if(missingFilter) {
					Debug.LogWarning("PhononGeometry without mesh data", audioGeo[i].gameObject);
				}
			}

			// Cleanup any mesh filters without meshes
			for(int i = filters.Count - 1; i >= 0; i--) {
				if(filters[i].sharedMesh == null) {
					Debug.LogWarning("PhononGeometry MeshFilter missing mesh", filters[i].gameObject);
					filters.RemoveAt(i);
				}
			}


			return filters;
		}

		SteamAudioManager phononManager = null;
        GameObject targetObject = null;
        Editor editor = null;
		OnRenderObjectHelper renderHelper = null;
	}
}