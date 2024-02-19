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
    public class ProbeArray
    {
        IntPtr mProbeArray = IntPtr.Zero;

        public ProbeArray(Context context)
        {
            API.iplProbeArrayCreate(context.Get(), out mProbeArray);
        }

        public ProbeArray(ProbeArray probeArray)
        {
            mProbeArray = API.iplProbeArrayRetain(probeArray.mProbeArray);
        }

        ~ProbeArray()
        {
            Release();
        }

        public void Release()
        {
            API.iplProbeArrayRelease(ref mProbeArray);
        }

        public IntPtr Get()
        {
            return mProbeArray;
        }

        public void GenerateProbes(Scene scene, ProbeGenerationParams probeGenerationParams)
        {
            API.iplProbeArrayGenerateProbes(mProbeArray, scene.Get(), ref probeGenerationParams);
        }

        public int GetNumProbes()
        {
            return API.iplProbeArrayGetNumProbes(mProbeArray);
        }

        public Sphere GetProbe(int index)
        {
            return API.iplProbeArrayGetProbe(mProbeArray, index);
        }
    }

    public class ProbeBatch
    {
        Context mContext = null;
        IntPtr mProbeBatch = IntPtr.Zero;

        public ProbeBatch(Context context)
        {
            mContext = context;

            API.iplProbeBatchCreate(context.Get(), out mProbeBatch);
        }

        public ProbeBatch(Context context, SerializedData dataAsset)
        {
            mContext = context;

            var serializedObject = new SerializedObject(context, dataAsset);

            var status = API.iplProbeBatchLoad(context.Get(), serializedObject.Get(), out mProbeBatch);
            if (status != Error.Success)
            {
                Debug.LogError(string.Format("Unable to load Probe Batch from {0}.", dataAsset.name));
                mProbeBatch = IntPtr.Zero;
            }

            serializedObject.Release();
        }

        public ProbeBatch(ProbeBatch probeBatch)
        {
            mContext = probeBatch.mContext;

            mProbeBatch = API.iplProbeBatchRetain(probeBatch.mProbeBatch);
        }

        ~ProbeBatch()
        {
            Release();
        }

        public void Release()
        {
            API.iplProbeBatchRelease(ref mProbeBatch);

            mContext = null;
        }

        public IntPtr Get()
        {
            return mProbeBatch;
        }

        public int Save(SerializedData dataAsset, bool flush = true)
        {
            var serializedObject = new SerializedObject(mContext);
            API.iplProbeBatchSave(mProbeBatch, serializedObject.Get());
            var size = (int) serializedObject.GetSize();
            serializedObject.WriteToFile(dataAsset, flush);
            serializedObject.Release();
            return size;
        }

        public void AddProbeArray(ProbeArray probeArray)
        {
            API.iplProbeBatchAddProbeArray(mProbeBatch, probeArray.Get());
        }

        public void Commit()
        {
            API.iplProbeBatchCommit(mProbeBatch);
        }

        public void RemoveData(BakedDataIdentifier identifier)
        {
            API.iplProbeBatchRemoveData(mProbeBatch, ref identifier);
        }

        public UIntPtr GetDataSize(BakedDataIdentifier identifier)
        {
            return API.iplProbeBatchGetDataSize(mProbeBatch, ref identifier);
        }
    }
}
#endif
