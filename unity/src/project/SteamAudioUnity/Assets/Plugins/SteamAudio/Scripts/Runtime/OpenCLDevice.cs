//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public class OpenCLDevice
    {
        IntPtr mOpenCLDevice = IntPtr.Zero;

        public OpenCLDevice(Context context, OpenCLDeviceType deviceType, int numCUsToReserve, float fractionCUsForIRUpdate,
            bool requiresTAN)
        {
            var deviceSettings = new OpenCLDeviceSettings { };
            deviceSettings.type = deviceType;
            deviceSettings.numCUsToReserve = numCUsToReserve;
            deviceSettings.fractionCUsForIRUpdate = fractionCUsForIRUpdate;
            deviceSettings.requiresTAN = requiresTAN ? Bool.True : Bool.False;

            var deviceList = IntPtr.Zero;
            var status = API.iplOpenCLDeviceListCreate(context.Get(), ref deviceSettings, out deviceList);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to enumerate OpenCL devices. [{0}]", status));

            var numDevices = API.iplOpenCLDeviceListGetNumDevices(deviceList);
            if (numDevices <= 0)
            {
                API.iplOpenCLDeviceListRelease(ref deviceList);

                // If we explicitly requested a device that supports TAN, or if we didn't ask for CU
                // reservation, but still didn't find any devices, stop.
                if (requiresTAN || numCUsToReserve == 0)
                    throw new Exception(string.Format("No OpenCL devices found."));

                Debug.LogWarning("No OpenCL devices found that match the provided parameters, attempting to " +
                    "initialize without CU reservation.");

                deviceSettings.numCUsToReserve = 0;
                deviceSettings.fractionCUsForIRUpdate = 0.0f;
                status = API.iplOpenCLDeviceListCreate(context.Get(), ref deviceSettings, out deviceList);
                if (status != Error.Success)
                    throw new Exception(string.Format("Unable to enumerate OpenCL devices. [{0}]", status));

                numDevices = API.iplOpenCLDeviceListGetNumDevices(deviceList);
                if (numDevices <= 0)
                    throw new Exception(string.Format("No OpenCL devices found."));
            }

            status = API.iplOpenCLDeviceCreate(context.Get(), deviceList, 0, out mOpenCLDevice);
            if (status != Error.Success)
            {
                API.iplOpenCLDeviceListRelease(ref deviceList);
                throw new Exception(string.Format("Unable to create OpenCL device. [{0}]", status));
            }

            API.iplOpenCLDeviceListRelease(ref deviceList);
        }

        public OpenCLDevice(OpenCLDevice device)
        {
            mOpenCLDevice = API.iplOpenCLDeviceRetain(device.mOpenCLDevice);
        }

        ~OpenCLDevice()
        {
            Release();
        }

        public void Release()
        {
            API.iplOpenCLDeviceRelease(ref mOpenCLDevice);
        }

        public IntPtr Get()
        {
            return mOpenCLDevice;
        }
    }
}
