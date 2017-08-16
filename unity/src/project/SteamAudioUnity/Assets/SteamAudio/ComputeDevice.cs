//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;

namespace SteamAudio
{
    public class ComputeDevice
    {
        public IntPtr GetDevice()
        {
            return device;
        }

        public Error Create(GlobalContext globalContext, bool useOpenCL, ComputeDeviceType deviceType, int numComputeUnits)
        {
            var error = Error.None;

            if (useOpenCL)
            {
                error = PhononCore.iplCreateComputeDevice(globalContext, deviceType, numComputeUnits, ref device);
                if (error != Error.None)
                {
                    throw new Exception("Unable to create OpenCL compute device (" + deviceType.ToString() + ", " +
                        numComputeUnits.ToString() + " CUs): [" + error.ToString() + "]");
                }
            }

            return error;
        }

        public void Destroy()
        {
            if (device != IntPtr.Zero)
                PhononCore.iplDestroyComputeDevice(ref device);
        }

        IntPtr device = IntPtr.Zero;
    }
}