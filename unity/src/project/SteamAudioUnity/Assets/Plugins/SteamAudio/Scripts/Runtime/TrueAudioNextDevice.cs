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
            Release();
        }

        public void Release()
        {
            API.iplTrueAudioNextDeviceRelease(ref mTrueAudioNextDevice);
        }

        public IntPtr Get()
        {
            return mTrueAudioNextDevice;
        }
    }
}
