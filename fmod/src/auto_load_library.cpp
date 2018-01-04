//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include <stdlib.h>
#include <stdio.h>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

#include "auto_load_library.h"
SteamAudioApi gApi;

#if defined(_WIN32) || defined(_WIN64)
#define PLUGIN_LIBRARY_NAME L"phonon_fmod.dll"
#define CORE_LIBRARY_NAME   L"phonon.dll"
#elif defined(__APPLE__)
#define PLUGIN_LIBRARY_NAME "audioplugin_phonon.bundle/Contents/MacOS/audioplugin_phonon"
#define CORE_LIBRARY_NAME   "phonon.bundle/Contents/MacOS/phonon_bundle"
#else
#define PLUGIN_LIBRARY_NAME "libaudioplugin_phonon.so"
#define CORE_LIBRARY_NAME   "libphonon.so"
#endif

/** Returns true if the given absolute path contains the given file name or relative path.
 */
template <typename T> bool pathContains(const T& path, const T& fileName)
{
    return path.find(fileName) != T::npos;
}

/** Returns the absolute path (including trailing slash or backslash) to the directory containing the given file.
 */
template <typename T> T basePath(const T& path, const T& fileName)
{
    return path.substr(0, path.find(fileName));
}

/** Returns the the absolute path (including trailing slash or backslash) to the Steam Audio dynamic library.
 */
#if defined(_WIN32) || defined(_WIN64)
std::wstring getLibraryPath()
{
    auto module = GetModuleHandle(PLUGIN_LIBRARY_NAME);

    wchar_t fileName[MAX_PATH] = { 0 };
    GetModuleFileName(module, fileName, MAX_PATH);

    auto directory = basePath(std::wstring{ fileName }, std::wstring{ PLUGIN_LIBRARY_NAME });
    auto libraryName = directory + std::wstring{ CORE_LIBRARY_NAME };

    return libraryName;
}
#elif defined(__APPLE__)
std::string getLibraryPath()
{
    auto directory = std::string{};

    auto numImages = _dyld_image_count();
    for (auto i = 0; i < numImages; ++i)
    {
        auto imagePath = std::string{ _dyld_get_image_name(i) };
        if (pathContains(imagePath, std::string(PLUGIN_LIBRARY_NAME)))
        {
            directory = basePath(imagePath, std::string(PLUGIN_LIBRARY_NAME));
            break;
        }
    }

    auto libraryName = directory + std::string{ CORE_LIBRARY_NAME };

    return libraryName;
}
#else
std::string getLibraryPath()
{
    auto directory = std::string{};

    auto maps = fopen("/proc/self/maps", "r");
    while (true)
    {
        char line[16384] = { 0 };
        fgets(line, sizeof(line), maps);
        if (feof(maps))
            break;

        auto mapping = std::string{ line };
        auto pathPosition = mapping.find_first_of("/");
        if (pathPosition < mapping.length())
        {
            mapping = mapping.substr(pathPosition);
            if (pathContains(mapping, std::string(PLUGIN_LIBRARY_NAME)))
            {
                directory = basePath(mapping, std::string(PLUGIN_LIBRARY_NAME));
                break;
            }
        }
    }
    fclose(maps);

    auto libraryName = directory + std::string{ CORE_LIBRARY_NAME };

    return libraryName;
}
#endif

/** Loads the Steam Audio dynamic library and binds all necessary function pointers.
 */
#if defined(_WIN32) || defined(_WIN64)
void loadLibrary()
{
    auto libraryPath = getLibraryPath();
    auto library = LoadLibrary(libraryPath.c_str());
    if (!library)
        return;

    gApi.iplCreateContext                   = (IPLCreateContext)                    GetProcAddress(library, "iplCreateContext");
    gApi.iplDestroyContext                  = (IPLDestroyContext)                   GetProcAddress(library, "iplDestroyContext");
    gApi.iplCalculateRelativeDirection      = (IPLCalculateRelativeDirection)       GetProcAddress(library, "iplCalculateRelativeDirection");
    gApi.iplCreateBinauralRenderer          = (IPLCreateBinauralRenderer)           GetProcAddress(library, "iplCreateBinauralRenderer");
    gApi.iplDestroyBinauralRenderer         = (IPLDestroyBinauralRenderer)          GetProcAddress(library, "iplDestroyBinauralRenderer");
    gApi.iplCreatePanningEffect             = (IPLCreatePanningEffect)              GetProcAddress(library, "iplCreatePanningEffect");
    gApi.iplDestroyPanningEffect            = (IPLDestroyPanningEffect)             GetProcAddress(library, "iplDestroyPanningEffect");
    gApi.iplApplyPanningEffect              = (IPLApplyPanningEffect)               GetProcAddress(library, "iplApplyPanningEffect");
    gApi.iplCreateBinauralEffect            = (IPLCreateBinauralEffect)             GetProcAddress(library, "iplCreateBinauralEffect");
    gApi.iplDestroyBinauralEffect           = (IPLDestroyBinauralEffect)            GetProcAddress(library, "iplDestroyBinauralEffect");
    gApi.iplApplyBinauralEffect             = (IPLApplyBinauralEffect)              GetProcAddress(library, "iplApplyBinauralEffect");
    gApi.iplCreateAmbisonicsPanningEffect   = (IPLCreateAmbisonicsPanningEffect)    GetProcAddress(library, "iplCreateAmbisonicsPanningEffect");
    gApi.iplDestroyAmbisonicsPanningEffect  = (IPLDestroyAmbisonicsPanningEffect)   GetProcAddress(library, "iplDestroyAmbisonicsPanningEffect");
    gApi.iplApplyAmbisonicsPanningEffect    = (IPLApplyAmbisonicsPanningEffect)     GetProcAddress(library, "iplApplyAmbisonicsPanningEffect");
    gApi.iplFlushAmbisonicsPanningEffect    = (IPLFlushAmbisonicsPanningEffect)     GetProcAddress(library, "iplFlushAmbisonicsPanningEffect");
    gApi.iplCreateAmbisonicsBinauralEffect  = (IPLCreateAmbisonicsBinauralEffect)   GetProcAddress(library, "iplCreateAmbisonicsBinauralEffect");
    gApi.iplDestroyAmbisonicsBinauralEffect = (IPLDestroyAmbisonicsBinauralEffect)  GetProcAddress(library, "iplDestroyAmbisonicsBinauralEffect");
    gApi.iplApplyAmbisonicsBinauralEffect   = (IPLApplyAmbisonicsBinauralEffect)    GetProcAddress(library, "iplApplyAmbisonicsBinauralEffect");
    gApi.iplFlushAmbisonicsBinauralEffect   = (IPLFlushAmbisonicsBinauralEffect)    GetProcAddress(library, "iplFlushAmbisonicsBinauralEffect");
    gApi.iplCreateEnvironmentalRenderer     = (IPLCreateEnvironmentalRenderer)      GetProcAddress(library, "iplCreateEnvironmentalRenderer");
    gApi.iplDestroyEnvironmentalRenderer    = (IPLDestroyEnvironmentalRenderer)     GetProcAddress(library, "iplDestroyEnvironmentalRenderer");
    gApi.iplGetDirectSoundPath              = (IPLGetDirectSoundPath)               GetProcAddress(library, "iplGetDirectSoundPath");
    gApi.iplCreateDirectSoundEffect         = (IPLCreateDirectSoundEffect)          GetProcAddress(library, "iplCreateDirectSoundEffect");
    gApi.iplDestroyDirectSoundEffect        = (IPLDestroyDirectSoundEffect)         GetProcAddress(library, "iplDestroyDirectSoundEffect");
    gApi.iplApplyDirectSoundEffect          = (IPLApplyDirectSoundEffect)           GetProcAddress(library, "iplApplyDirectSoundEffect");
    gApi.iplCreateConvolutionEffect         = (IPLCreateConvolutionEffect)          GetProcAddress(library, "iplCreateConvolutionEffect");
    gApi.iplDestroyConvolutionEffect        = (IPLDestroyConvolutionEffect)         GetProcAddress(library, "iplDestroyConvolutionEffect");
    gApi.iplSetConvolutionEffectIdentifier  = (IPLSetConvolutionEffectIdentifier)   GetProcAddress(library, "iplSetConvolutionEffectIdentifier");
    gApi.iplSetDryAudioForConvolutionEffect = (IPLSetDryAudioForConvolutionEffect)  GetProcAddress(library, "iplSetDryAudioForConvolutionEffect");
    gApi.iplGetWetAudioForConvolutionEffect = (IPLGetWetAudioForConvolutionEffect)  GetProcAddress(library, "iplGetWetAudioForConvolutionEffect");
    gApi.iplGetMixedEnvironmentalAudio      = (IPLGetMixedEnvironmentalAudio)       GetProcAddress(library, "iplGetMixedEnvironmentalAudio");
    gApi.iplFlushConvolutionEffect          = (IPLFlushConvolutionEffect)           GetProcAddress(library, "iplFlushConvolutionEffect");
}
#else
void loadLibrary()
{
    auto libraryPath = getLibraryPath();
    auto library = dlopen(libraryPath.c_str(), RTLD_LAZY);
    if (!library)
        return;

    gApi.iplCreateContext                   = (IPLCreateContext)                    dlsym(library, "iplCreateContext");
    gApi.iplDestroyContext                  = (IPLDestroyContext)                   dlsym(library, "iplDestroyContext");
    gApi.iplCalculateRelativeDirection      = (IPLCalculateRelativeDirection)       dlsym(library, "iplCalculateRelativeDirection");
    gApi.iplCreateBinauralRenderer          = (IPLCreateBinauralRenderer)           dlsym(library, "iplCreateBinauralRenderer");
    gApi.iplDestroyBinauralRenderer         = (IPLDestroyBinauralRenderer)          dlsym(library, "iplDestroyBinauralRenderer");
    gApi.iplCreatePanningEffect             = (IPLCreatePanningEffect)              dlsym(library, "iplCreatePanningEffect");
    gApi.iplDestroyPanningEffect            = (IPLDestroyPanningEffect)             dlsym(library, "iplDestroyPanningEffect");
    gApi.iplApplyPanningEffect              = (IPLApplyPanningEffect)               dlsym(library, "iplApplyPanningEffect");
    gApi.iplCreateBinauralEffect            = (IPLCreateBinauralEffect)             dlsym(library, "iplCreateBinauralEffect");
    gApi.iplDestroyBinauralEffect           = (IPLDestroyBinauralEffect)            dlsym(library, "iplDestroyBinauralEffect");
    gApi.iplApplyBinauralEffect             = (IPLApplyBinauralEffect)              dlsym(library, "iplApplyBinauralEffect");
    gApi.iplCreateAmbisonicsPanningEffect   = (IPLCreateAmbisonicsPanningEffect)    dlsym(library, "iplCreateAmbisonicsPanningEffect");
    gApi.iplDestroyAmbisonicsPanningEffect  = (IPLDestroyAmbisonicsPanningEffect)   dlsym(library, "iplDestroyAmbisonicsPanningEffect");
    gApi.iplApplyAmbisonicsPanningEffect    = (IPLApplyAmbisonicsPanningEffect)     dlsym(library, "iplApplyAmbisonicsPanningEffect");
    gApi.iplFlushAmbisonicsPanningEffect    = (IPLFlushAmbisonicsPanningEffect)     dlsym(library, "iplFlushAmbisonicsPanningEffect");
    gApi.iplCreateAmbisonicsBinauralEffect  = (IPLCreateAmbisonicsBinauralEffect)   dlsym(library, "iplCreateAmbisonicsBinauralEffect");
    gApi.iplDestroyAmbisonicsBinauralEffect = (IPLDestroyAmbisonicsBinauralEffect)  dlsym(library, "iplDestroyAmbisonicsBinauralEffect");
    gApi.iplApplyAmbisonicsBinauralEffect   = (IPLApplyAmbisonicsBinauralEffect)    dlsym(library, "iplApplyAmbisonicsBinauralEffect");
    gApi.iplFlushAmbisonicsBinauralEffect   = (IPLFlushAmbisonicsBinauralEffect)    dlsym(library, "iplFlushAmbisonicsBinauralEffect");
    gApi.iplCreateEnvironmentalRenderer     = (IPLCreateEnvironmentalRenderer)      dlsym(library, "iplCreateEnvironmentalRenderer");
    gApi.iplDestroyEnvironmentalRenderer    = (IPLDestroyEnvironmentalRenderer)     dlsym(library, "iplDestroyEnvironmentalRenderer");
    gApi.iplGetDirectSoundPath              = (IPLGetDirectSoundPath)               dlsym(library, "iplGetDirectSoundPath");
    gApi.iplCreateDirectSoundEffect         = (IPLCreateDirectSoundEffect)          dlsym(library, "iplCreateDirectSoundEffect");
    gApi.iplDestroyDirectSoundEffect        = (IPLDestroyDirectSoundEffect)         dlsym(library, "iplDestroyDirectSoundEffect");
    gApi.iplApplyDirectSoundEffect          = (IPLApplyDirectSoundEffect)           dlsym(library, "iplApplyDirectSoundEffect");
    gApi.iplCreateConvolutionEffect         = (IPLCreateConvolutionEffect)          dlsym(library, "iplCreateConvolutionEffect");
    gApi.iplDestroyConvolutionEffect        = (IPLDestroyConvolutionEffect)         dlsym(library, "iplDestroyConvolutionEffect");
    gApi.iplSetConvolutionEffectIdentifier  = (IPLSetConvolutionEffectIdentifier)   dlsym(library, "iplSetConvolutionEffectIdentifier");
    gApi.iplSetDryAudioForConvolutionEffect = (IPLSetDryAudioForConvolutionEffect)  dlsym(library, "iplSetDryAudioForConvolutionEffect");
    gApi.iplGetWetAudioForConvolutionEffect = (IPLGetWetAudioForConvolutionEffect)  dlsym(library, "iplGetWetAudioForConvolutionEffect");
    gApi.iplGetMixedEnvironmentalAudio      = (IPLGetMixedEnvironmentalAudio)       dlsym(library, "iplGetMixedEnvironmentalAudio");
    gApi.iplFlushConvolutionEffect          = (IPLFlushConvolutionEffect)           dlsym(library, "iplFlushConvolutionEffect");
}
#endif

/** When the Steam Audio Unity plugin library is loaded, load the Steam Audio core library.
 */
#if defined(_WIN32) || defined(_WIN64)
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        loadLibrary();

    return TRUE;
}
#else
__attribute__((constructor))
static void onLoad()
{
    loadLibrary();
}
#endif