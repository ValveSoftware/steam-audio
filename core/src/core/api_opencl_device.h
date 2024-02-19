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

#pragma once

#include "error.h"
#include "context.h"
#include "profiler.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// COpenCLDeviceList
// --------------------------------------------------------------------------------------------------------------------

class COpenCLDeviceList : public IOpenCLDeviceList
{
public:
    Handle<OpenCLDeviceList> mHandle;

    COpenCLDeviceList(CContext* context,
                      IPLOpenCLDeviceSettings* settings);

    virtual IOpenCLDeviceList* retain() override;

    virtual void release() override;

    virtual IPLint32 getNumDevices() override;

    virtual void getDeviceDesc(IPLint32 index,
                               IPLOpenCLDeviceDesc* deviceDesc) override;
};


// --------------------------------------------------------------------------------------------------------------------
// COpenCLDevice
// --------------------------------------------------------------------------------------------------------------------

class COpenCLDevice : public IOpenCLDevice
{
public:
    Handle<OpenCLDevice> mHandle;

    COpenCLDevice(CContext* context,
                  IOpenCLDeviceList* deviceList,
                  IPLint32 index);

    COpenCLDevice(CContext* context,
                  void* convolutionQueue,
                  void* irUpdateQueue);

    virtual IOpenCLDevice* retain() override;

    virtual void release() override;

    virtual IPLerror createRadeonRaysDevice(IPLRadeonRaysDeviceSettings* settings,
                                            IRadeonRaysDevice** device) override;

    virtual IPLerror createTrueAudioNextDevice(IPLTrueAudioNextDeviceSettings* settings,
                                               ITrueAudioNextDevice** device) override;
};

}