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

using System;
using UnityEngine;

namespace SteamAudio
{
    public class InstancedMesh
    {
        IntPtr mInstancedMesh = IntPtr.Zero;

        public InstancedMesh(Scene scene, Scene subScene, Transform transform)
        {
            var instancedMeshSettings = new InstancedMeshSettings { };
            instancedMeshSettings.subScene = subScene.Get();
            instancedMeshSettings.transform = Common.ConvertTransform(transform);

            var status = API.iplInstancedMeshCreate(scene.Get(), ref instancedMeshSettings, out mInstancedMesh);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create instanced mesh ({0}). [{1}]", transform.gameObject.name, status));
        }

        public InstancedMesh(InstancedMesh instancedMesh)
        {
            mInstancedMesh = API.iplInstancedMeshRetain(instancedMesh.mInstancedMesh);
        }

        ~InstancedMesh()
        {
            Release();
        }

        public void Release()
        {
            API.iplInstancedMeshRelease(ref mInstancedMesh);
        }

        public IntPtr Get()
        {
            return mInstancedMesh;
        }

        public void AddToScene(Scene scene)
        {
            API.iplInstancedMeshAdd(mInstancedMesh, scene.Get());
            scene.NotifyAddObject();
        }

        public void RemoveFromScene(Scene scene)
        {
            API.iplInstancedMeshRemove(mInstancedMesh, scene.Get());
            scene.NotifyRemoveObject();
        }

        public void UpdateTransform(Scene scene, Transform transform)
        {
            API.iplInstancedMeshUpdateTransform(mInstancedMesh, scene.Get(), Common.ConvertTransform(transform));
        }
    }
}

#endif
