//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace SteamAudio
{
    public class HRTF
    {
        IntPtr mHRTF = IntPtr.Zero;

        public HRTF(Context context, AudioSettings audioSettings, string sofaFileName, byte[] sofaFileData, float gaindB, HRTFNormType normType)
        {
            IntPtr sofaData = IntPtr.Zero;

            var hrtfSettings = new HRTFSettings { };
            if (sofaFileData != null && sofaFileData.Length > 0)
            {
                hrtfSettings.type = HRTFType.SOFA;

                sofaData = Marshal.AllocHGlobal(sofaFileData.Length);
                Marshal.Copy(sofaFileData, 0, sofaData, sofaFileData.Length);

                hrtfSettings.sofaFileData = sofaData;
                hrtfSettings.sofaFileDataSize = sofaFileData.Length;
            }
            else if (sofaFileName != null)
            {
                hrtfSettings.type = HRTFType.SOFA;
                hrtfSettings.sofaFileName = sofaFileName;
            }
            else
            {
                hrtfSettings.type = HRTFType.Default;
            }

            hrtfSettings.volume = dBToGain(gaindB);
            hrtfSettings.normType = normType;

            var status = API.iplHRTFCreate(context.Get(), ref audioSettings, ref hrtfSettings, out mHRTF);
            if (status != Error.Success)
            {
                Debug.LogError(string.Format("Unable to load HRTF: {0}. [{1}]", (sofaFileName != null) ? sofaFileName : "default", status));
                mHRTF = IntPtr.Zero;
            }
            else
            {
                Debug.Log(string.Format("Loaded HRTF: {0}.", (sofaFileName != null) ? sofaFileName : "default"));
            }

            if (sofaData != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(sofaData);
            }
        }

        public HRTF(HRTF hrtf)
        {
            mHRTF = API.iplHRTFRetain(hrtf.Get());
        }

        ~HRTF()
        {
            Release();
        }

        public void Release()
        {
            API.iplHRTFRelease(ref mHRTF);
        }

        public IntPtr Get()
        {
            return mHRTF;
        }

        private float dBToGain(float gaindB)
        {
            const float kMinDBLevel = -90.0f;

            if (gaindB <= kMinDBLevel)
                return 0.0f;

            return  (float) Math.Pow(10.0, (double) gaindB * (1.0 / 20.0));
        }
    }
}
