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
            Release();
        }

        public void Release()
        {
            API.iplRadeonRaysDeviceRelease(ref mRadeonRaysDevice);
        }

        public IntPtr Get()
        {
            return mRadeonRaysDevice;
        }
    }
}
