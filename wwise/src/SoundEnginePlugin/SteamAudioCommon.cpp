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

#include "SteamAudioCommon.h"

#include <assert.h>
#include <math.h>

#if defined(IPL_OS_IOS) || defined(IPL_OS_WASM)
#define STEAMAUDIO_BUILDING_CORE
#endif
#include <phonon_interfaces.h>

namespace SteamAudioWwise {

// --------------------------------------------------------------------------------------------------------------------
// SourceMap
// --------------------------------------------------------------------------------------------------------------------

void SourceMap::Add(AkGameObjectID gameObjectID, IPLSource source)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (map.find(gameObjectID) == map.end())
    {
        map[gameObjectID] = std::make_shared<DoubleBufferedSource>();
    }

    map[gameObjectID]->Write(source);
}


void SourceMap::Remove(AkGameObjectID gameObjectID)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (map.find(gameObjectID) != map.end())
    {
        map.erase(gameObjectID);
    }
}


std::shared_ptr<DoubleBufferedSource> SourceMap::Get(AkGameObjectID gameObjectID)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (map.find(gameObjectID) != map.end())
        return map[gameObjectID];
    else
        return nullptr;
}


// --------------------------------------------------------------------------------------------------------------------
// GlobalState<T>
// --------------------------------------------------------------------------------------------------------------------

GlobalState::GlobalState()
    : library{nullptr}
    , iplContextCreateFn{nullptr}
    , context{}
    , globalPluginContext{nullptr}
    , hrtf{}
    , sourceMap{}
    , refCount{0}
    , metersPerUnit{1.0f}
    , simulationSettings{}
    , simulationSettingsValid{false}
    , reflectionMixer{}
    , reverbSource{}
{}


void GlobalState::Retain()
{
    ++refCount;
}


void GlobalState::Release()
{
    if (--refCount == 0)
    {
        hrtf.Reset();
        context.Reset();
    }
}


GlobalState& GlobalState::Get()
{
    static GlobalState global{};
    return global;
}


// --------------------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------------------

#if defined(IPL_OS_WINDOWS)
constexpr wchar_t const* STEAMAUDIO_WWISE_EXE_NAME        = L"Wwise.exe";
constexpr wchar_t const* STEAMAUDIO_PLUGIN_DLL_NAMES[]    = {L"SteamAudioWwise.dll"};
constexpr wchar_t const* STEAMAUDIO_DLL_NAMES[]           = {L"phonon.dll"};
#elif defined(IPL_OS_MACOSX)
constexpr char const* STEAMAUDIO_WWISE_EXE_NAME           = "Wwise.app";
constexpr char const* STEAMAUDIO_PLUGIN_DLL_NAMES[]       = {"libSteamAudioWwise.dylib", "SteamAudioWwise.bundle/Contents/MacOS/SteamAudioWwise"};
constexpr char const* STEAMAUDIO_DLL_NAMES[]              = {"libphonon.dylib", "phonon.bundle/Contents/MacOS/phonon"};
#endif

constexpr char const* STEAMAUDIO_FUNCTION_NAME            = "iplContextCreate";

#if !defined(IPL_OS_WINDOWS)
#define MAX_PATH 260
#endif


/**
 * Returns the absolute path to the dynamic library file containing the Steam Audio Wwise plugin code.
 * This will only be called when running under the Wwise editor.
 */
#if defined(IPL_OS_WINDOWS)
static void GetPluginLibraryPath(wchar_t* path, int pathLength)
{
    pathLength = std::min(pathLength, MAX_PATH);

    auto numPluginLibraryNames = sizeof(STEAMAUDIO_PLUGIN_DLL_NAMES) / sizeof(STEAMAUDIO_PLUGIN_DLL_NAMES[0]);

    for (auto i = 0; i < numPluginLibraryNames; ++i)
    {
        auto library = GetModuleHandle(STEAMAUDIO_PLUGIN_DLL_NAMES[i]);
        if (!library)
            continue;

        GetModuleFileName(library, path, MAX_PATH);

        auto pos = wcsstr(path, STEAMAUDIO_PLUGIN_DLL_NAMES[i]);
        if (pos)
        {
            *pos = '\0';
        }

        return;
    }
}
#elif defined(IPL_OS_MACOSX)
static void GetPluginLibraryPath(char* path, int pathLength)
{
    auto numPluginLibraryNames = sizeof(STEAMAUDIO_PLUGIN_DLL_NAMES) / sizeof(STEAMAUDIO_PLUGIN_DLL_NAMES[0]);

    auto numImages = _dyld_image_count();
    for (auto i = 0; i < numImages; ++i)
    {
        auto imagePath = _dyld_get_image_name(i);
        for (auto j = 0; j < numPluginLibraryNames; ++j)
        {
            if (auto pos = strstr(imagePath, STEAMAUDIO_PLUGIN_DLL_NAMES[j]))
            {
                auto numChars = pos - imagePath;
                memcpy(path, imagePath, numChars);
                path[numChars] = '\0';
                return;
            }
        }
    }
}
#endif


/**
 * Returns the absolute path to the dynamic library file containing the Steam Audio SDK (e.g. phonon.dll).
 * This is expected to be in the same directory as the Steam Audio Wwise plugin DLL.
 * This will only be called when running under the Wwise editor.
 */
#if defined(IPL_OS_WINDOWS)
static void GetCoreLibraryPath(const wchar_t* name, wchar_t* path, int pathLength)
{
    GetPluginLibraryPath(path, pathLength);
    wcsncat(path, name, wcslen(name));
}
#elif defined(IPL_OS_MACOSX)
static void GetCoreLibraryPath(const char* name, char* path, int pathLength)
{
    GetPluginLibraryPath(path, pathLength);
    strncat(path, name, strlen(name));
}
#endif


/**
 * Loads the dynamic library with the given name from the same directory as the dynamic library containing
 * this code.
 * This will only be called when running under the Wwise editor.
 */
#if defined(IPL_OS_WINDOWS)
static bool LoadLibraryFromFile(const wchar_t* name)
{
    auto& globalState = GlobalState::Get();

    wchar_t path[MAX_PATH] = {0};
    GetCoreLibraryPath(name, path, MAX_PATH);

    globalState.library = LoadLibraryEx(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (globalState.library)
        return true;

    globalState.library = LoadLibraryEx(name, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (globalState.library)
        return true;

    return false;
}
#elif defined(IPL_OS_MACOSX)
static bool LoadLibraryFromFile(const char* name)
{
    auto& globalState = GlobalState::Get();

    char path[MAX_PATH] = {0};
    GetCoreLibraryPath(name, path, MAX_PATH);

    globalState.library = dlopen(path, RTLD_LAZY);
    if (globalState.library)
        return true;

    return false;
}
#endif


/**
 * Returns a pointer to the function with the given name from the given dynamic library.
 * This will only be called when running under the Wwise editor.
 */
#if defined(IPL_OS_WINDOWS)
static void* GetFunctionFromLibrary(HMODULE library, const char* name)
{
    return GetProcAddress(library, name);
}
#elif defined(IPL_OS_MACOSX)
static void* GetFunctionFromLibrary(void* library, const char* name)
{
    return dlsym(library, name);
}
#endif


/**
 * Loads the Steam Audio SDK dynamic library and looks up a pointer to the iplContextCreate function.
 * This will only be called when running under the Wwise editor.
 */
#if defined(IPL_OS_WINDOWS) || defined(IPL_OS_MACOSX)
static bool LoadSteamAudioLibrary()
{
    auto& globalState = GlobalState::Get();

    auto numCoreLibraryNames = sizeof(STEAMAUDIO_DLL_NAMES) / sizeof(STEAMAUDIO_DLL_NAMES[0]);

    for (auto i = 0; i < numCoreLibraryNames; ++i)
    {
        if (!LoadLibraryFromFile(STEAMAUDIO_DLL_NAMES[i]))
            continue;

        globalState.iplContextCreateFn = (decltype(iplContextCreate)*) GetFunctionFromLibrary(globalState.library, STEAMAUDIO_FUNCTION_NAME);

        return true;
    }

    return false;
}
#else
static bool LoadSteamAudioLibrary()
{
    return false;
}
#endif


bool IsRunningInEditor()
{
#if defined(IPL_OS_WINDOWS)
    wchar_t moduleFileName[MAX_PATH] = {0};
    GetModuleFileName(nullptr, moduleFileName, MAX_PATH);
    return (wcsstr(moduleFileName, STEAMAUDIO_WWISE_EXE_NAME) != nullptr);
#elif defined(IPL_OS_MACOSX)
    char moduleFileName[1024] = {0};
    uint32_t bufferSize = 1024;
    _NSGetExecutablePath(moduleFileName, &bufferSize);
    return (strstr(moduleFileName, STEAMAUDIO_WWISE_EXE_NAME) != nullptr);
#else
    return false;
#endif
}


static void IPLCALL WwiseLog(IPLLogLevel level, const char* message)
{
    auto& globalState = GlobalState::Get();

    AK::IAkGlobalPluginContext* globalPluginContext = globalState.globalPluginContext;
    if (!globalPluginContext)
        return;

    AK::Monitor::ErrorLevel errorLevel = (level == IPL_LOGLEVEL_ERROR) ? AK::Monitor::ErrorLevel_Error : AK::Monitor::ErrorLevel_Message;
    globalPluginContext->PostMonitorMessage(message, errorLevel);
}


static void* IPLCALL WwiseAllocate(IPLsize size, IPLsize alignment)
{
    auto& globalState = GlobalState::Get();

    AK::IAkGlobalPluginContext* globalPluginContext = globalState.globalPluginContext;
    if (!globalPluginContext)
        return nullptr;

    AK::IAkPluginMemAlloc* allocator = globalPluginContext->GetAllocator();
    if (!allocator)
        return nullptr;

    return AK_PLUGIN_ALLOC_ALIGN(allocator, size, alignment);
}


static void IPLCALL WwiseFree(void* memoryBlock)
{
    if (!memoryBlock)
        return;

    auto& globalState = GlobalState::Get();

    AK::IAkGlobalPluginContext* globalPluginContext = globalState.globalPluginContext;
    if (!globalPluginContext)
        return;

    AK::IAkPluginMemAlloc* allocator = globalPluginContext->GetAllocator();
    if (!allocator)
        return;

    AK_PLUGIN_FREE(allocator, memoryBlock);
}


bool EnsureSteamAudioContextExists(IPLAudioSettings& in_audioSettings, AK::IAkGlobalPluginContext* in_pGlobalPluginContext)
{
    auto& globalState = GlobalState::Get();

    if (globalState.globalPluginContext == nullptr)
    {
        globalState.globalPluginContext = in_pGlobalPluginContext;
    }

    if (globalState.context.Read())
        return true;

    if (!IsRunningInEditor())
        return false;

    if (!LoadSteamAudioLibrary())
        return false;

    IPLContextSettings contextSettings{};
    contextSettings.version = STEAMAUDIO_VERSION;
    contextSettings.logCallback = WwiseLog;
    contextSettings.allocateCallback = WwiseAllocate;
    contextSettings.freeCallback = WwiseFree;
    contextSettings.simdLevel = IPL_SIMDLEVEL_AVX2;

    IPLContext context = nullptr;
    auto status = globalState.iplContextCreateFn(&contextSettings, &context);
    if (status != IPL_STATUS_SUCCESS)
        return false;

    IPLHRTFSettings hrtfSettings{};
    hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;
    hrtfSettings.volume = 1.0f;

    IPLHRTF hrtf = nullptr;
    status = iplHRTFCreate(context, &in_audioSettings, &hrtfSettings, &hrtf);
    if (status != IPL_STATUS_SUCCESS)
        return false;

    globalState.context.Write(context);
    globalState.hrtf.Write(hrtf);
    
    iplHRTFRelease(&hrtf);
    iplContextRelease(&context);

    return true;
}


IPLSpeakerLayout SpeakerLayoutForNumChannels(int numChannels)
{
    IPLSpeakerLayout speakerLayout;
    speakerLayout.numSpeakers = numChannels;
    speakerLayout.speakers = nullptr;

    if (numChannels == 1)
        speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_MONO;
    else if (numChannels == 2)
        speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
    else if (numChannels == 4)
        speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_QUADRAPHONIC;
    else if (numChannels == 6)
        speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_5_1;
    else if (numChannels == 8)
        speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_7_1;
    else
        speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_CUSTOM;

    return speakerLayout;
}


int OrderForNumChannels(int numChannels)
{
    return static_cast<int>(sqrtf(static_cast<float>(numChannels))) - 1;
}


int NumChannelsForOrder(int order)
{
    return (order + 1) * (order + 1);
}


int NumSamplesForDuration(float duration, int samplingRate)
{
    return static_cast<int>(ceilf(duration * samplingRate));
}


IPLVector3 ConvertVector(const AkVector64& vec)
{
    return IPLVector3{static_cast<float>(vec.X), static_cast<float>(vec.Y), -static_cast<float>(vec.Z)};
}

float Dot(const IPLVector3& a, const IPLVector3& b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

IPLVector3 UnitVector(IPLVector3 v)
{
    auto length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 1e-2f)
        length = 1e-2f;

    return IPLVector3{v.x / length, v.y / length, v.z / length};
}


IPLVector3 CrossVector(const IPLVector3& a, const IPLVector3& b)
{
    IPLVector3 c;
    c.x = a.y * b.z - a.z * b.y;
    c.y = a.z * b.x - a.x * b.z;
    c.z = a.x * b.y - a.y * b.x;
    return c;
}


IPLCoordinateSpace3 CalculateCoordinates(AkWorldTransform& transform)
{
    IPLCoordinateSpace3 out;
    out.origin = ConvertVector(transform.Position());
    out.ahead = ConvertVector(transform.OrientationFront());
    out.up = ConvertVector(transform.OrientationTop());
    out.right = UnitVector(CrossVector(out.ahead, out.up));

    auto& globalState = GlobalState::Get();
    out.origin.x *= globalState.metersPerUnit;
    out.origin.y *= globalState.metersPerUnit;
    out.origin.z *= globalState.metersPerUnit;

    return out;
}


void ApplyVolumeRamp(float volume, float& prevVolume, const IPLAudioBuffer& audioBuffer)
{
    for (auto i = 0; i < audioBuffer.numChannels; ++i)
    {
        for (auto j = 0; j < audioBuffer.numSamples; ++j)
        {
            auto fraction = static_cast<float>(j) / static_cast<float>(audioBuffer.numSamples);
            audioBuffer.data[i][j] *= fraction * volume + (1.0f - fraction) * prevVolume;
        }
    }

    prevVolume = volume;
}

}


// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

void AKSOUNDENGINE_CALL iplWwiseGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch)
{
    if (major)
        *major = STEAMAUDIO_WWISE_VERSION_MAJOR;
    if (minor)
        *minor = STEAMAUDIO_WWISE_VERSION_MINOR;
    if (patch)
        *patch = STEAMAUDIO_WWISE_VERSION_PATCH;
}


void AKSOUNDENGINE_CALL iplWwiseInitialize(IPLContext context, IPLWwiseSettings* settings)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    globalState.Retain();

    assert(globalState.context.Read() == nullptr);
    globalState.context.Write(context);

    if (!settings)
        return;

    globalState.metersPerUnit = settings->metersPerUnit;
}


void AKSOUNDENGINE_CALL iplWwiseTerminate()
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    globalState.Release();
}


void AKSOUNDENGINE_CALL iplWwiseSetHRTF(IPLHRTF hrtf)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    globalState.hrtf.Write(hrtf);
}


void AKSOUNDENGINE_CALL iplWwiseSetSimulationSettings(IPLSimulationSettings simulationSettings)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    globalState.simulationSettings = simulationSettings;
    globalState.simulationSettingsValid = true;
}


void AKSOUNDENGINE_CALL iplWwiseSetReverbSource(IPLSource reverbSource)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    globalState.reverbSource.Write(reverbSource);
}


void AKSOUNDENGINE_CALL iplWwiseAddSource(AkGameObjectID gameObjectID, IPLSource source)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    globalState.sourceMap.Add(gameObjectID, source);
}


void AKSOUNDENGINE_CALL iplWwiseRemoveSource(AkGameObjectID gameObjectID)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    globalState.sourceMap.Remove(gameObjectID);
}
