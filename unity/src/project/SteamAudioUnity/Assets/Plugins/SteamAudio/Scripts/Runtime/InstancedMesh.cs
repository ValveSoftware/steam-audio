//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

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
