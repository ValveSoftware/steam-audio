//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
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