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

#include "opencl_device.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_opencl_device.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// COpenCLDeviceList
// --------------------------------------------------------------------------------------------------------------------

COpenCLDeviceList::COpenCLDeviceList(CContext* context,
                                     IPLOpenCLDeviceSettings* settings)
{
#if defined(IPL_USES_OPENCL)
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _type = static_cast<OpenCLDeviceType>(settings->type);
    auto _requiresTAN = (settings->requiresTAN == IPL_TRUE);

    new (&mHandle) Handle<OpenCLDeviceList>(ipl::make_shared<OpenCLDeviceList>(_type, settings->numCUsToReserve, settings->fractionCUsForIRUpdate, _requiresTAN), _context);
#endif
}

IOpenCLDeviceList* COpenCLDeviceList::retain()
{
#if defined(IPL_USES_OPENCL)
    mHandle.retain();
    return this;
#else
    return nullptr;
#endif
}

void COpenCLDeviceList::release()
{
#if defined(IPL_USES_OPENCL)
    if (mHandle.release())
    {
        this->~COpenCLDeviceList();
        gMemory().free(this);
    }
#endif
}

IPLint32 COpenCLDeviceList::getNumDevices()
{
#if defined(IPL_USES_OPENCL)
    auto _deviceList = mHandle.get();
    if (!_deviceList)
        return 0;

    return _deviceList->numDevices();
#else
    return 0;
#endif
}

void COpenCLDeviceList::getDeviceDesc(IPLint32 index,
                                      IPLOpenCLDeviceDesc* deviceDesc)
{
#if defined(IPL_USES_OPENCL)
    if (index < 0 || !deviceDesc)
        return;

    auto _deviceList = mHandle.get();
    if (!_deviceList)
        return;

    auto& _deviceDesc = (*_deviceList)[index];

    deviceDesc->platform = _deviceDesc.platform;
    deviceDesc->platformName = _deviceDesc.platformName.c_str();
    deviceDesc->platformVendor = _deviceDesc.platformVendor.c_str();
    deviceDesc->platformVersion = _deviceDesc.platformVersion.c_str();
    deviceDesc->device = _deviceDesc.device;
    deviceDesc->deviceName = _deviceDesc.deviceName.c_str();
    deviceDesc->deviceVendor = _deviceDesc.deviceVendor.c_str();
    deviceDesc->deviceVersion = _deviceDesc.deviceVersion.c_str();
    deviceDesc->type = static_cast<IPLOpenCLDeviceType>(_deviceDesc.type);
    deviceDesc->numConvolutionCUs = _deviceDesc.numConvolutionCUs;
    deviceDesc->numIRUpdateCUs = _deviceDesc.numIRUpdateCUs;
    deviceDesc->granularity = _deviceDesc.cuReservationGranularity;
    deviceDesc->perfScore = _deviceDesc.perfScore;
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// COpenCLDevice
// --------------------------------------------------------------------------------------------------------------------

COpenCLDevice::COpenCLDevice(CContext* context,
                             IOpenCLDeviceList* deviceList,
                             IPLint32 index)
{
#if defined(IPL_USES_OPENCL)
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _deviceList = static_cast<COpenCLDeviceList*>(deviceList)->mHandle.get();
    if (!_deviceList)
        throw Exception(Status::Failure);

    auto& _deviceDesc = (*_deviceList)[index];

    new (&mHandle) Handle<OpenCLDevice>(ipl::make_shared<OpenCLDevice>(_deviceDesc.platform, _deviceDesc.device, _deviceDesc.numConvolutionCUs, _deviceDesc.numIRUpdateCUs), _context);
#endif
}

COpenCLDevice::COpenCLDevice(CContext* context,
                             void* convolutionQueue,
                             void* irUpdateQueue)
{
#if defined(IPL_USES_OPENCL)
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _convolutionQueue = static_cast<cl_command_queue>(convolutionQueue);
    auto _irUpdateQueue = static_cast<cl_command_queue>(irUpdateQueue);

    new (&mHandle) Handle<OpenCLDevice>(ipl::make_shared<OpenCLDevice>(_convolutionQueue, _irUpdateQueue), _context);
#endif
}

IOpenCLDevice* COpenCLDevice::retain()
{
#if defined(IPL_USES_OPENCL)
    mHandle.retain();
    return this;
#else
    return nullptr;
#endif
}

void COpenCLDevice::release()
{
#if defined(IPL_USES_OPENCL)
    if (mHandle.release())
    {
        this->~COpenCLDevice();
        gMemory().free(this);
    }
#endif
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createOpenCLDeviceList(IPLOpenCLDeviceSettings* settings,
                                          IOpenCLDeviceList** deviceList)
{
#if defined(IPL_USES_OPENCL)
    if (!settings || !deviceList)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _deviceList = reinterpret_cast<COpenCLDeviceList*>(gMemory().allocate(sizeof(COpenCLDeviceList), Memory::kDefaultAlignment));
        new (_deviceList) COpenCLDeviceList(this, settings);
        *deviceList = _deviceList;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
#else
    return IPL_STATUS_FAILURE;
#endif
}

IPLerror CContext::createOpenCLDevice(IOpenCLDeviceList* deviceList,
                                      IPLint32 index,
                                      IOpenCLDevice** device)
{
#if defined(IPL_USES_OPENCL)
    if (!deviceList || index < 0 || !device)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _device = reinterpret_cast<COpenCLDevice*>(gMemory().allocate(sizeof(COpenCLDevice), Memory::kDefaultAlignment));
        new (_device) COpenCLDevice(this, deviceList, index);
        *device = _device;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
#else
    return IPL_STATUS_FAILURE;
#endif
}

IPLerror CContext::createOpenCLDeviceFromExisting(void* convolutionQueue,
                                                  void* irUpdateQueue,
                                                  IOpenCLDevice** device)
{
#if defined(IPL_USES_OPENCL)
    if (!convolutionQueue || !irUpdateQueue || !device)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _device = reinterpret_cast<COpenCLDevice*>(gMemory().allocate(sizeof(COpenCLDevice), Memory::kDefaultAlignment));
        new (_device) COpenCLDevice(this, convolutionQueue, irUpdateQueue);
        *device = _device;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
#else
    return IPL_STATUS_FAILURE;
#endif
}

}
