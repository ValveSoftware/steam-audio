//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;

namespace SteamAudio
{
    public class TrueAudioNextDevice
    {
        IntPtr mTrueAudioNextDevice = IntPtr.Zero;

        public TrueAudioNextDevice(OpenCLDevice openCLDevice, int frameSize, int irSize, int order, int maxSources)
        {
            var deviceSettings = new TrueAudioNextDeviceSettings { };
            deviceSettings.frameSize = frameSize;
            deviceSettings.irSize = irSize;
            deviceSettings.order = order;
            deviceSettings.maxSources = maxSources;

            var status = API.iplTrueAudioNextDeviceCreate(openCLDevice.Get(), ref deviceSettings, out mTrueAudioNextDevice);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to create TrueAudio Next device. [{0}]", status));
        }

        public TrueAudioNextDevice(TrueAudioNextDevice device)
        {
            mTrueAudioNextDevice = API.iplTrueAudioNextDeviceRetain(device.mTrueAudioNextDevice);
        }

        ~TrueAudioNextDevice()
        {
            API.iplTrueAudioNextDeviceRelease(ref mTrueAudioNextDevice);
        }

        public IntPtr Get()
        {
            return mTrueAudioNextDevice;
        }
    }
}
