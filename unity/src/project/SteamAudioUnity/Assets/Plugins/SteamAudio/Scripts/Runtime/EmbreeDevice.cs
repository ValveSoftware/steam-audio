//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;

namespace SteamAudio
{
    public class EmbreeDevice
    {
        IntPtr mEmbreeDevice = IntPtr.Zero;

        public EmbreeDevice(Context context)
        {
            var embreeDeviceSettings = new EmbreeDeviceSettings { };

            var status = API.iplEmbreeDeviceCreate(context.Get(), ref embreeDeviceSettings, out mEmbreeDevice);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create Embree device. [{0}]", status));
        }

        public EmbreeDevice(EmbreeDevice embreeDevice)
        {
            mEmbreeDevice = API.iplEmbreeDeviceRetain(embreeDevice.mEmbreeDevice);
        }

        ~EmbreeDevice()
        {
            Release();
        }

        public void Release()
        {
            API.iplEmbreeDeviceRelease(ref mEmbreeDevice);
        }

        public IntPtr Get()
        {
            return mEmbreeDevice;
        }
    }
}
