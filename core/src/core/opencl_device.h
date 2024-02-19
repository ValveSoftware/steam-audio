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

#if defined(IPL_USES_OPENCL)

#include <CL/cl.h>

#include "array.h"
#include "containers.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLDeviceList
// --------------------------------------------------------------------------------------------------------------------

enum class OpenCLDeviceType
{
    Any,
    CPU,
    GPU
};

struct OpenCLDeviceDesc
{
    cl_platform_id platform;
    string platformName;
    string platformVendor;
    string platformVersion;
    cl_device_id device;
    string deviceName;
    string deviceVendor;
    string deviceVersion;
    OpenCLDeviceType type;
    int numConvolutionCUs;
    int numIRUpdateCUs;
    int cuReservationGranularity;
    float perfScore;
};

class OpenCLDeviceList
{
public:
    OpenCLDeviceList(OpenCLDeviceType type,
                     int numCUsToReserve,
                     float fractionCUsForIRUpdate,
                     bool requiresTAN);

    int numDevices() const
    {
        return static_cast<int>(mDevices.size());
    }

    const OpenCLDeviceDesc& operator[](int i) const
    {
        return mDevices[i];
    }

private:
    vector<OpenCLDeviceDesc> mDevices;

    void enumerateOpenCL(OpenCLDeviceType type);

#if defined(IPL_USES_TRUEAUDIONEXT)
    void enumerateAMF(OpenCLDeviceType type,
                      int numCUsToReserve,
                      float fractionCUsForIRUpdate,
                      bool requiresTAN);
#endif
};


// --------------------------------------------------------------------------------------------------------------------
// OpenCLDevice
// --------------------------------------------------------------------------------------------------------------------

class OpenCLDevice
{
public:
    OpenCLDevice(cl_platform_id platform,
                 cl_device_id device,
                 int numConvolutionCUs,
                 int numIRUpdateCUs);

    OpenCLDevice(cl_command_queue convolutionQueue,
                 cl_command_queue irUpdateQueue);

    ~OpenCLDevice();

    cl_platform_id platform() const
    {
        return mPlatform;
    }

    cl_device_id device() const
    {
        return mDevice;
    }

    cl_context context() const
    {
        return mContext;
    }

    cl_command_queue convolutionQueue() const
    {
        return mConvolutionQueue;
    }

    cl_command_queue irUpdateQueue() const
    {
        return mIRUpdateQueue;
    }

    size_t paddedSize(size_t size) const;

    static cl_platform_id platformForDevice(cl_device_id device);

    static string platformName(cl_platform_id platform);

    static string platformVendor(cl_platform_id platform);

    static string platformVersion(cl_platform_id platform);

    static string deviceName(cl_device_id device);

    static string deviceVendor(cl_device_id device);

    static string deviceVersion(cl_device_id device);

private:
    cl_platform_id mPlatform;
    cl_device_id mDevice;
    cl_context mContext;
    cl_command_queue mConvolutionQueue;
    cl_command_queue mIRUpdateQueue;
    void* mAMF;
    cl_uint mMemAlignment;

    void initOpenCL();

#if defined(IPL_USES_TRUEAUDIONEXT)
    void initAMF(int numConvolutionCUs,
                 int numIRUpdateCUs);
#endif

    static string platformInfoString(cl_platform_id platform,
                                     cl_platform_info info);

    static string deviceInfoString(cl_device_id device,
                                   cl_device_info info);
};

}

#else

namespace ipl {

class OpenCLDevice
{};

}

#endif
