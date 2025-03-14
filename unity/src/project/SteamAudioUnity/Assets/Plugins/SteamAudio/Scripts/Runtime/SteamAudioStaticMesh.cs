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

using System.Threading.Tasks;

using UnityEngine;

namespace SteamAudio
{
    public class SteamAudioStaticMesh : MonoBehaviour
    {
        [Header("Export Settings")]
        public SerializedData asset = null;
        public string sceneNameWhenExported = "";

#if STEAMAUDIO_ENABLED
        StaticMesh mStaticMesh = null;
        Task<StaticMesh> mTask = null;

        void Start()
        {
            if (asset == null)
            {
                Debug.LogWarningFormat("No asset set for Steam Audio Static Mesh in scene {0}. Export the scene before clicking Play.",
                    gameObject.scene.name);
            }
        }

        void OnDestroy()
        {
            if (mStaticMesh != null)
            {
                mStaticMesh.Release();
            }
            else if (mTask != null)
            {
                mTask.ContinueWith(static e => e.Result.Release());
            }
        }

        void OnEnable()
        {
            if (mStaticMesh != null)
            {
                mStaticMesh.AddToScene(SteamAudioManager.CurrentScene);
                SteamAudioManager.ScheduleCommitScene();
            }
        }

        void OnDisable()
        {
            if (mStaticMesh != null && SteamAudioManager.CurrentScene != null)
            {
                mStaticMesh.RemoveFromScene(SteamAudioManager.CurrentScene);
                SteamAudioManager.ScheduleCommitScene();
            }
        }

        void Update()
        {
            if (mStaticMesh == null && asset != null)
            {
                if (mTask == null)
                {
                    mTask = Task.Run(() => new StaticMesh(SteamAudioManager.Context, SteamAudioManager.CurrentScene, asset));
                }
                else if (mTask.IsCompleted)
                {
                    mStaticMesh = mTask.Result;
                    mTask = null;
                    if (enabled)
                    {
                        mStaticMesh.AddToScene(SteamAudioManager.CurrentScene);
                        SteamAudioManager.ScheduleCommitScene();
                    }
                }
            }
        }
#endif
    }
}
