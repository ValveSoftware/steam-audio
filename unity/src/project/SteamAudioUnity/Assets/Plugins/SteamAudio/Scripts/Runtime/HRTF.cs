//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public class HRTF
    {
        IntPtr mHRTF = IntPtr.Zero;

        public HRTF(Context context, AudioSettings audioSettings, string sofaFileName)
        {
            var hrtfSettings = new HRTFSettings { };
            hrtfSettings.type = (sofaFileName != null) ? HRTFType.SOFA : HRTFType.Default;
            hrtfSettings.sofaFileName = sofaFileName;

            var status = API.iplHRTFCreate(context.Get(), ref audioSettings, ref hrtfSettings, out mHRTF);
            if (status != Error.Success)
            {
                Debug.LogError(string.Format("Unable to load HRTF: {0}.", hrtfSettings.sofaFileName));
                mHRTF = IntPtr.Zero;
            }
            else
            {
                Debug.Log(string.Format("Loaded HRTF: {0}.", (sofaFileName != null) ? hrtfSettings.sofaFileName : "default"));
            }
        }

        public HRTF(HRTF hrtf)
        {
            mHRTF = API.iplHRTFRetain(hrtf.Get());
        }

        ~HRTF()
        {
            API.iplHRTFRelease(ref mHRTF);
        }

        public IntPtr Get()
        {
            return mHRTF;
        }
    }
}
