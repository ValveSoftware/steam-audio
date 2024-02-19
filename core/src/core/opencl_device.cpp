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

#if defined(IPL_USES_OPENCL)

#include "opencl_device.h"

#include <Windows.h>

#if defined(IPL_USES_TRUEAUDIONEXT)
#include <public/include/core/Factory.h>
#include <GpuUtilities.h>
#endif

#include "log.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLDeviceList
// --------------------------------------------------------------------------------------------------------------------

OpenCLDeviceList::OpenCLDeviceList(OpenCLDeviceType type,
                                   int numCUsToReserve,
                                   float fractionCUsForIRUpdate,
                                   bool requiresTAN)
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    if (numCUsToReserve > 0 || requiresTAN)
    {
        enumerateAMF(type, numCUsToReserve, fractionCUsForIRUpdate, requiresTAN);
    }
    else
#endif
    {
        enumerateOpenCL(type);
    }
}

void OpenCLDeviceList::enumerateOpenCL(OpenCLDeviceType type)
{
    cl_uint numPlatforms = 0;
    auto status = clGetPlatformIDs(0, nullptr, &numPlatforms);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    if (numPlatforms <= 0)
        throw Exception(Status::Initialization);

    Array<cl_platform_id> platforms(numPlatforms);
    status = clGetPlatformIDs(numPlatforms, platforms.data(), nullptr);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    auto _type = CL_DEVICE_TYPE_ALL;
    if (type == OpenCLDeviceType::CPU)
    {
        _type = CL_DEVICE_TYPE_CPU;
    }
    else if (type == OpenCLDeviceType::GPU)
    {
        _type = CL_DEVICE_TYPE_GPU;
    }

    for (auto i = 0u; i < numPlatforms; ++i)
    {
        cl_uint numDevices = 0;
        status = clGetDeviceIDs(platforms[i], _type, 0, nullptr, &numDevices);

        if (numDevices <= 0)
            continue;

        if (status != CL_SUCCESS)
            throw Exception(Status::Initialization);
 
        Array<cl_device_id> devices(numDevices);
        status = clGetDeviceIDs(platforms[i], _type, numDevices, devices.data(), nullptr);
        if (status != CL_SUCCESS)
            throw Exception(Status::Initialization);

        for (auto j = 0u; j < numDevices; ++j)
        {
            OpenCLDeviceDesc deviceDesc;
            deviceDesc.platform = platforms[i];
            deviceDesc.platformName = OpenCLDevice::platformName(platforms[i]);
            deviceDesc.platformVendor = OpenCLDevice::platformVendor(platforms[i]);
            deviceDesc.platformVersion = OpenCLDevice::platformVersion(platforms[i]);
            deviceDesc.device = devices[j];
            deviceDesc.deviceName = OpenCLDevice::deviceName(devices[j]);
            deviceDesc.deviceVendor = OpenCLDevice::deviceVendor(devices[j]);
            deviceDesc.deviceVersion = OpenCLDevice::deviceVersion(devices[j]);
            deviceDesc.type = type;
            deviceDesc.numConvolutionCUs = 0;
            deviceDesc.numIRUpdateCUs = 0;
            deviceDesc.cuReservationGranularity = 0;
            deviceDesc.perfScore = 0.0f;

            mDevices.push_back(deviceDesc);
        }
    }
}

#if defined(IPL_USES_TRUEAUDIONEXT)
void OpenCLDeviceList::enumerateAMF(OpenCLDeviceType type,
                                    int numCUsToReserve,
                                    float fractionCUsForIRUpdate,
                                    bool requiresTAN)
{
    assert(numCUsToReserve > 0 || requiresTAN);
    assert(0.0f <= fractionCUsForIRUpdate && fractionCUsForIRUpdate <= 1.0f);

    auto numDevices = 0;
    TanDeviceCapabilities* deviceCaps = nullptr;
    listTanDevicesAndCaps(&deviceCaps, &numDevices);

    auto _type = CL_DEVICE_TYPE_ALL;
    if (type == OpenCLDeviceType::CPU)
    {
        _type = CL_DEVICE_TYPE_CPU;
    }
    else if (type == OpenCLDeviceType::GPU)
    {
        _type = CL_DEVICE_TYPE_GPU;
    }

    for (auto i = 0; i < numDevices; ++i)
    {
        // If we specifically want either a CPU or GPU device, ignore all other kinds of devices.
        if (type != OpenCLDeviceType::Any && deviceCaps[i].devType != _type)
            continue;

        // If we require TAN support, skip devices that don't support TAN.
        if ((numCUsToReserve > 0 || requiresTAN) && !deviceCaps[i].supportsTAN)
            continue;

        // This is the range of #CUs that can be reserved on this device.
        auto maxCUs = deviceCaps[i].maxReservableComputeUnits;
        auto granularity = deviceCaps[i].reserveComputeUnitsGranularity;

        // If the requested #CUs is valid but the device doesn't support CU reservation, skip it.
        if (maxCUs == 0 || granularity == 0)
            continue;

        // If the maximum requested #CUs are less than minimum allocable CUs.
        if (numCUsToReserve < granularity)
            continue;

        numCUsToReserve = std::min(numCUsToReserve, maxCUs);

        auto cuFractionConvolution = (1.0f - fractionCUsForIRUpdate) * numCUsToReserve;
        auto cuFractionIRUpdate = fractionCUsForIRUpdate * numCUsToReserve;

        // The case where CUs cannot be split at all despite requiring reserved queues for TAN and IR Update.
        if (cuFractionConvolution > 0.0f && cuFractionIRUpdate > 0.0f && granularity == numCUsToReserve)
            continue;

        int numConvolutionCUs = 0;
        int numIRUpdateCUs = 0;

        if (fractionCUsForIRUpdate == 0.0f)
        {
            numConvolutionCUs = numCUsToReserve;
            numIRUpdateCUs = 0;
        }
        else if (fractionCUsForIRUpdate == 1.0f)
        {
            numConvolutionCUs = 0;
            numIRUpdateCUs = numCUsToReserve;
        }
        else if (cuFractionConvolution < static_cast<float>(granularity))
        {
            numConvolutionCUs = granularity;
            numIRUpdateCUs = numCUsToReserve - granularity;
        }
        else if (cuFractionIRUpdate < static_cast<float>(granularity))
        {
            numConvolutionCUs = numCUsToReserve - granularity;
            numIRUpdateCUs = granularity;
        }
        else
        {
            numConvolutionCUs = static_cast<int>(ceilf(cuFractionConvolution) / granularity) * granularity;
            numIRUpdateCUs = numCUsToReserve - numConvolutionCUs;
        }

        OpenCLDeviceDesc deviceDesc;
        deviceDesc.platform = OpenCLDevice::platformForDevice(deviceCaps[i].devId);
        deviceDesc.platformName = OpenCLDevice::platformName(deviceDesc.platform);
        deviceDesc.platformVendor = OpenCLDevice::platformVendor(deviceDesc.platform);
        deviceDesc.platformVersion = OpenCLDevice::platformVersion(deviceDesc.platform);
        deviceDesc.device = deviceCaps[i].devId;
        deviceDesc.deviceName = OpenCLDevice::deviceName(deviceCaps[i].devId);
        deviceDesc.deviceVendor = OpenCLDevice::deviceVendor(deviceCaps[i].devId);;
        deviceDesc.deviceVersion = OpenCLDevice::deviceVersion(deviceCaps[i].devId);;
        deviceDesc.type = type;
        deviceDesc.numConvolutionCUs = numConvolutionCUs;
        deviceDesc.numIRUpdateCUs = numIRUpdateCUs;
        deviceDesc.cuReservationGranularity = granularity;
        deviceDesc.perfScore = deviceCaps[i].ComputeUnitPerfFactor;

        mDevices.push_back(deviceDesc);
    }

    // IMPORTANT NOTE: If building in debug mode, ensure that GpuUtilities.dll was also built in debug mode,
    // otherwise heap corruption could result here if the delete[] is executed.  The debug version of delete[] looks for
    // "no man's land" padding around the actual memory block, so allocating something in release mode and
    // freeing it in debug mode messes up the address calculations used for heap verification.
#if !(defined(DEBUG) || defined(_DEBUG))
    delete[] deviceCaps;
#endif
}
#endif


// --------------------------------------------------------------------------------------------------------------------
// OpenCLDevice
// --------------------------------------------------------------------------------------------------------------------

OpenCLDevice::OpenCLDevice(cl_platform_id platform,
                           cl_device_id device,
                           int numConvolutionCUs,
                           int numIRUpdateCUs)
    : mPlatform(platform)
    , mDevice(device)
    , mAMF(nullptr)
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    if (numConvolutionCUs > 0 || numIRUpdateCUs > 0)
    {
        initAMF(numConvolutionCUs, numIRUpdateCUs);
    }
    else
#endif
    {
        initOpenCL();
    }

    auto _platformName = platformName(mPlatform);
    auto _platformVendor = platformVendor(mPlatform);
    auto _platformVersion = platformVersion(mPlatform);
    gLog().message(MessageSeverity::Info, "Initialized OpenCL platform: %s %s (%s).", _platformVendor.c_str(),
                   _platformName.c_str(), _platformVersion.c_str());

    auto _deviceName = deviceName(mDevice);
    auto _deviceVendor = deviceVendor(mDevice);
    auto _deviceVersion = deviceVersion(mDevice);
    gLog().message(MessageSeverity::Info, "Initialized OpenCL device: %s %s (%s).", _deviceVendor.c_str(),
                   _deviceName.c_str(), _deviceVersion.c_str());

    clGetDeviceInfo(mDevice, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(cl_uint), &mMemAlignment, nullptr);
}

OpenCLDevice::OpenCLDevice(cl_command_queue convolutionQueue,
                           cl_command_queue irUpdateQueue)
    : mConvolutionQueue(convolutionQueue)
    , mIRUpdateQueue(irUpdateQueue)
    , mAMF(nullptr)
{
    clRetainCommandQueue(mConvolutionQueue);
    clRetainCommandQueue(mIRUpdateQueue);

    clGetCommandQueueInfo(mConvolutionQueue, CL_QUEUE_CONTEXT, sizeof(cl_context), &mContext, nullptr);
    clRetainContext(mContext);

    clGetCommandQueueInfo(mConvolutionQueue, CL_QUEUE_DEVICE, sizeof(cl_device_id), &mDevice, nullptr);

    clGetDeviceInfo(mDevice, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &mPlatform, nullptr);

    auto _platformName = platformName(mPlatform);
    auto _platformVendor = platformVendor(mPlatform);
    auto _platformVersion = platformVersion(mPlatform);
    gLog().message(MessageSeverity::Info, "Using OpenCL platform: %s %s (%s).", _platformVendor.c_str(),
                   _platformName.c_str(), _platformVersion.c_str());

    auto _deviceName = deviceName(mDevice);
    auto _deviceVendor = deviceVendor(mDevice);
    auto _deviceVersion = deviceVersion(mDevice);
    gLog().message(MessageSeverity::Info, "Using OpenCL device: %s %s (%s).", _deviceVendor.c_str(),
                   _deviceName.c_str(), _deviceVersion.c_str());

    clGetDeviceInfo(mDevice, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(cl_uint), &mMemAlignment, nullptr);
}

void OpenCLDevice::initOpenCL()
{
    auto status = CL_SUCCESS;

    mContext = clCreateContext(nullptr, 1, &mDevice, nullptr, nullptr, &status);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    mConvolutionQueue = clCreateCommandQueue(mContext, mDevice, 0, &status);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);

    mIRUpdateQueue = clCreateCommandQueue(mContext, mDevice, 0, &status);
    if (status != CL_SUCCESS)
        throw Exception(Status::Initialization);
}

#if defined(IPL_USES_TRUEAUDIONEXT)
void OpenCLDevice::initAMF(int numConvolutionCUs, int numIRUpdateCUs)
{
    mAMF = LoadLibrary(AMF_DLL_NAME);
    if (!mAMF)
        throw Exception(Status::Initialization);

    auto AMFQueryVersion = reinterpret_cast<AMFQueryVersion_Fn>(GetProcAddress(reinterpret_cast<HMODULE>(mAMF), AMF_QUERY_VERSION_FUNCTION_NAME));
    auto AMFInit = reinterpret_cast<AMFInit_Fn>(GetProcAddress(reinterpret_cast<HMODULE>(mAMF), AMF_INIT_FUNCTION_NAME));
    if (!AMFQueryVersion || !AMFInit)
        throw Exception(Status::Initialization);

    uint64_t version = 0;
    auto status = AMFQueryVersion(&version);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    auto versionMajor = (version >> 48);
    auto versionMinor = (version >> 32) & 0xffff;
    auto versionRelease = (version >> 16) & 0xffff;
    auto versionBuildNum = version & 0xffff;
    gLog().message(MessageSeverity::Info, "Initialized AMD Advanced Media Framework v%d.%d.%d.%d.",
                   versionMajor, versionMinor, versionRelease, versionBuildNum);

    amf::AMFFactory* factory = nullptr;
    status = AMFInit(version, &factory);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    amf::AMFContextPtr context = nullptr;
    status = factory->CreateContext(&context);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    amf::AMFComputeFactoryPtr computeFactory = nullptr;
    status = context->GetOpenCLComputeFactory(&computeFactory);
    if (status != AMF_OK)
        throw Exception(Status::Initialization);

    auto numDevices = computeFactory->GetDeviceCount();
    if (numDevices == 0)
        throw Exception(Status::Initialization);

    for (auto i = 0; i < numDevices; ++i)
    {
        amf::AMFComputeDevicePtr computeDevice = nullptr;
        status = computeFactory->GetDeviceAt(i, &computeDevice);
        if (status != AMF_OK)
            throw Exception(Status::Initialization);

        if (static_cast<cl_device_id>(computeDevice->GetNativeDeviceID()) == mDevice)
        {
            status = context->InitOpenCLEx(computeDevice);
            if (status != AMF_OK)
                throw Exception(Status::Initialization);

            mContext = static_cast<cl_context>(computeDevice->GetNativeContext());
            clRetainContext(mContext);

            auto kRealTimePriority = 1;
            auto kMediumPriority = 2;

            amf::AMFComputePtr convolutionQueue = nullptr;
            amf::AMFComputePtr irUpdateQueue = nullptr;

            computeDevice->SetProperty(L"MaxRealTimeComputeUnits", numConvolutionCUs);
            computeDevice->SetProperty(L"MaxRealTimeComputeUnits2", numIRUpdateCUs);

            status = computeDevice->CreateCompute(&kRealTimePriority, &convolutionQueue);
            if (status != AMF_OK)
                throw Exception(Status::Initialization);

            status = computeDevice->CreateCompute(&kMediumPriority, &irUpdateQueue);
            if (status != AMF_OK)
                throw Exception(Status::Initialization);

            mConvolutionQueue = static_cast<cl_command_queue>(convolutionQueue->GetNativeCommandQueue());
            mIRUpdateQueue = static_cast<cl_command_queue>(irUpdateQueue->GetNativeCommandQueue());
            clRetainCommandQueue(mConvolutionQueue);
            clRetainCommandQueue(mIRUpdateQueue);

            break;
        }
    }

    gLog().message(MessageSeverity::Info, "Using %d CUs for convolution, %d CUs for IR update.", numConvolutionCUs,
                   numIRUpdateCUs);
}
#endif

OpenCLDevice::~OpenCLDevice()
{
    clReleaseCommandQueue(mIRUpdateQueue);
    clReleaseCommandQueue(mConvolutionQueue);
    clReleaseContext(mContext);

    if (mAMF)
    {
        FreeLibrary(reinterpret_cast<HMODULE>(mAMF));
    }
}

size_t OpenCLDevice::paddedSize(size_t size) const
{
    auto stride = size;
    if (size % mMemAlignment != 0)
    {
        stride = ((size / mMemAlignment) + 1) * mMemAlignment;
    }

    return stride;
}

string OpenCLDevice::platformName(cl_platform_id platform)
{
    return platformInfoString(platform, CL_PLATFORM_NAME);
}

string OpenCLDevice::platformVendor(cl_platform_id platform)
{
    return platformInfoString(platform, CL_PLATFORM_VENDOR);
}

string OpenCLDevice::platformVersion(cl_platform_id platform)
{
    return platformInfoString(platform, CL_PLATFORM_VERSION);
}

cl_platform_id OpenCLDevice::platformForDevice(cl_device_id device)
{
    cl_platform_id platform;
    clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr);
    return platform;
}

string OpenCLDevice::deviceName(cl_device_id device)
{
    return deviceInfoString(device, CL_DEVICE_NAME);
}

string OpenCLDevice::deviceVendor(cl_device_id device)
{
    return deviceInfoString(device, CL_DEVICE_VENDOR);
}

string OpenCLDevice::deviceVersion(cl_device_id device)
{
    return deviceInfoString(device, CL_DEVICE_VERSION);
}

string OpenCLDevice::platformInfoString(cl_platform_id platform,
                                        cl_platform_info info)
{
    char out[1024] = { 0 };
    clGetPlatformInfo(platform, info, sizeof(out), out, nullptr);
    return string(out);
}

string OpenCLDevice::deviceInfoString(cl_device_id device,
                                      cl_device_info info)
{
    char out[1024] = { 0 };
    clGetDeviceInfo(device, info, sizeof(out), out, nullptr);
    return string(out);
}

}

#endif
