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

namespace SteamAudio
{
    public class Scene
    {
        Context mContext = null;
        IntPtr mScene = IntPtr.Zero;
        int mNumObjects = 0;

        public Scene(Context context, SceneType type, EmbreeDevice embreeDevice, RadeonRaysDevice radeonRaysDevice, ClosestHitCallback closestHitCallback, AnyHitCallback anyHitCallback)
        {
            mContext = context;

            var sceneSettings = new SceneSettings { };
            sceneSettings.type = type;
            sceneSettings.embreeDevice = (embreeDevice != null) ? embreeDevice.Get() : IntPtr.Zero;
            sceneSettings.radeonRaysDevice = (radeonRaysDevice != null) ? radeonRaysDevice.Get() : IntPtr.Zero;
            sceneSettings.closestHitCallback = closestHitCallback;
            sceneSettings.anyHitCallback = anyHitCallback;

            var status = API.iplSceneCreate(context.Get(), ref sceneSettings, out mScene);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create scene [{0}]", status.ToString()));
        }

        public Scene(Context context, SceneSettings sceneSettings, SerializedData dataAsset)
        {
            mContext = context;

            var serializedObject = new SerializedObject(context, dataAsset);
            var status = API.iplSceneLoad(context.Get(), ref sceneSettings, serializedObject.Get(), null, IntPtr.Zero, out mScene);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to load scene [{0}]", status.ToString()));

            serializedObject.Release();
        }

        public Scene(Scene scene)
        {
            mContext = scene.mContext;

            mScene = API.iplSceneRetain(scene.mScene);
        }

        ~Scene()
        {
            Release();
        }

        public void Release()
        {
            API.iplSceneRelease(ref mScene);

            mContext = null;
        }

        public IntPtr Get()
        {
            return mScene;
        }

        public void Save(SerializedData dataAsset)
        {
            var serializedObject = new SerializedObject(mContext);
            API.iplSceneSave(mScene, serializedObject.Get());
            serializedObject.WriteToFile(dataAsset);
            serializedObject.Release();
        }

        public void SaveOBJ(string fileBaseName)
        {
            API.iplSceneSaveOBJ(mScene, fileBaseName);
        }

        public void NotifyAddObject()
        {
            ++mNumObjects;
        }

        public void NotifyRemoveObject()
        {
            --mNumObjects;
        }

        public int GetNumObjects()
        {
            return mNumObjects;
        }

        public void Commit()
        {
            API.iplSceneCommit(mScene);
        }
    }
}
#endif
