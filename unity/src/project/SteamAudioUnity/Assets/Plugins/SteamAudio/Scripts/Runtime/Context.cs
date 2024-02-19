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

using AOT;
using System;
using UnityEngine;

namespace SteamAudio
{
    public class Context
    {
        IntPtr mContext = IntPtr.Zero;

        public Context()
        {
            var contextSettings = new ContextSettings { };
            contextSettings.version = Constants.kVersion;
            contextSettings.logCallback = LogMessage;
            contextSettings.simdLevel = SIMDLevel.AVX2;

            if (SteamAudioSettings.Singleton.EnableValidation)
            {
                contextSettings.flags = contextSettings.flags | ContextFlags.Validation;
            }

            var status = API.iplContextCreate(ref contextSettings, out mContext);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create context. [{0}]", status));
        }

        public Context(Context context)
        {
            mContext = API.iplContextRetain(context.Get());
        }

        ~Context()
        {
            Release();
        }

        public void Release()
        {
            API.iplContextRelease(ref mContext);
        }

        public IntPtr Get()
        {
            return mContext;
        }

        [MonoPInvokeCallback(typeof(LogCallback))]
        public static void LogMessage(LogLevel level, string message)
        {
            switch (level)
            {
            case LogLevel.Info:
            case LogLevel.Debug:
                Debug.Log(message);
                break;
            case LogLevel.Warning:
                Debug.LogWarning(message);
                break;
            case LogLevel.Error:
                Debug.LogError(message);
                break;
            default:
                break;
            }
        }
    }
}