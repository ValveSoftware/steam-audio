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
