//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;

namespace SteamAudio
{
    public class RadeonRaysDevice
    {
        IntPtr mRadeonRaysDevice = IntPtr.Zero;

        public RadeonRaysDevice(OpenCLDevice openCLDevice)
        {
            var deviceSettings = new RadeonRaysDeviceSettings { };

            var status = API.iplRadeonRaysDeviceCreate(openCLDevice.Get(), ref deviceSettings, out mRadeonRaysDevice);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create Radeon Rays device. [{0}]", status));
        }

        public RadeonRaysDevice(RadeonRaysDevice device)
        {
            mRadeonRaysDevice = API.iplRadeonRaysDeviceRetain(device.mRadeonRaysDevice);
        }

        ~RadeonRaysDevice()
        {
            API.iplRadeonRaysDeviceRelease(ref mRadeonRaysDevice);
        }

        public IntPtr Get()
        {
            return mRadeonRaysDevice;
        }
    }
}
