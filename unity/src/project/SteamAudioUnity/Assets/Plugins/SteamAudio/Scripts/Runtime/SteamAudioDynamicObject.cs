//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Dynamic Object")]
    public class SteamAudioDynamicObject : MonoBehaviour
    {
        [Header("Export Settings")]
        public SerializedData asset = null;

        InstancedMesh mInstancedMesh = null;

        private void OnDestroy()
        {
            SteamAudioManager.UnloadDynamicObject(this);
        }

        private void OnEnable()
        {
            if (mInstancedMesh != null)
            {
                mInstancedMesh.AddToScene(SteamAudioManager.CurrentScene);
            }
        }

        private void OnDisable()
        {
            if (mInstancedMesh != null && SteamAudioManager.CurrentScene != null)
            {
                mInstancedMesh.RemoveFromScene(SteamAudioManager.CurrentScene);
            }
        }

        private void Update()
        {
            if (mInstancedMesh == null && asset != null)
            {
                mInstancedMesh = SteamAudioManager.LoadDynamicObject(this, SteamAudioManager.CurrentScene, SteamAudioManager.Context);

                if (enabled)
                {
                    mInstancedMesh.AddToScene(SteamAudioManager.CurrentScene);
                }
            }

            mInstancedMesh.UpdateTransform(SteamAudioManager.CurrentScene, transform);
        }
    }
}
