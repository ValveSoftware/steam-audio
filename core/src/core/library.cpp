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

#include "library.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#if defined(IPL_OS_WINDOWS)
#include <delayimp.h>
#endif

#include "error.h"

#if defined(IPL_OS_WINDOWS)
decltype(__pfnDliNotifyHook2) __pfnDliNotifyHook2 = delayLoadHook;
decltype(__pfnDliFailureHook2) __pfnDliFailureHook2 = delayLoadHook;
#endif

#if !defined(IPL_OS_WINDOWS)
#define MAX_PATH 260
#endif

namespace ipl {

#if defined(IPL_OS_WINDOWS)
const wchar_t* gLoadingBinaryNames[] = {
    L"phonon.dll",
    L"phonon_test.exe",
    L"phonon_itest.exe",
    L"phonon_perf.exe",
};
#elif defined (IPL_OS_MACOSX)
const char* gLoadingBinaryNames[] = {
    "libphonon.dylib",
    "phonon.bundle/Contents/MacOS/phonon",
    "phonon_test",
    "phonon_perf",
};
#else
const char* gLoadingBinaryNames[] = {
    "libphonon.so",
    "phonon_test",
    "phonon_perf",
};
#endif

#if defined(IPL_OS_WINDOWS)
void Library::getLoadingBinaryPath(char* loadingBinaryPath,
                                   int maxPathLength)
{
    maxPathLength = std::min(maxPathLength, MAX_PATH);

    auto numLoadingBinaryNames = sizeof(gLoadingBinaryNames) / sizeof(gLoadingBinaryNames[0]);

    HMODULE loadingBinary{};
    const wchar_t* loadingBinaryName{};
    for (auto i = 0u; i < numLoadingBinaryNames; ++i)
    {
        loadingBinary = GetModuleHandle(gLoadingBinaryNames[i]);
        if (loadingBinary)
        {
            loadingBinaryName = gLoadingBinaryNames[i];
            break;
        }
    }

    if (!loadingBinary)
        throw Exception(Status::Initialization);

    wchar_t loadingBinaryPathW[MAX_PATH] = {0};
    GetModuleFileName(loadingBinary, loadingBinaryPathW, MAX_PATH);

    auto pos = wcsstr(loadingBinaryPathW, loadingBinaryName);
    *pos = '\0';

    wcstombs_s(nullptr, loadingBinaryPath, maxPathLength, loadingBinaryPathW, _TRUNCATE);
}
#elif defined(IPL_OS_MACOSX)
void Library::getLoadingBinaryPath(char* loadingBinaryPath,
                                   int maxPathLength)
{
    auto numLoadingBinaryNames = sizeof(gLoadingBinaryNames) / sizeof(gLoadingBinaryNames[0]);

    auto numImages = _dyld_image_count();
    for (auto i = 0; i < numImages; ++i)
    {
        auto imagePath = _dyld_get_image_name(i);
        for (auto j = 0; j < numLoadingBinaryNames; ++j)
        {
            if (auto pos = strstr(imagePath, gLoadingBinaryNames[j]))
            {
                auto numChars = pos - imagePath;
                memcpy(loadingBinaryPath, imagePath, numChars);
                loadingBinaryPath[numChars] = '\0';
                return;
            }
        }
    }
}
#else
void Library::getLoadingBinaryPath(char* loadingBinaryPath,
                                   int maxPathLength)
{
    auto numLoadingBinaryNames = sizeof(gLoadingBinaryNames) / sizeof(gLoadingBinaryNames[0]);

    auto maps = fopen("/proc/self/maps", "r");

    while (true)
    {
        char line[16384] = {0};
        if (!fgets(line, sizeof(line), maps))
            break;

        auto path = strchr(line, '/');
        if (!path)
            break;

        for (auto i = 0; i < numLoadingBinaryNames; ++i)
        {
            if (auto pos = strstr(path, gLoadingBinaryNames[i]))
            {
                *pos = '\0';
                strncpy(loadingBinaryPath, path, pos - path + 1);
                fclose(maps);
                return;
            }
        }
    }

    fclose(maps);
}
#endif

void Library::getLoadedBinaryPath(const char* name,
                                  char* loadedBinaryPath,
                                  int maxPathLength)
{
    getLoadingBinaryPath(loadedBinaryPath, maxPathLength);
    strncat(loadedBinaryPath, name, strlen(name));
}

#if defined(IPL_OS_WINDOWS)
HMODULE Library::load(const char* name)
{
    char path[MAX_PATH] = {0};
    getLoadedBinaryPath(name, path, MAX_PATH);

    wchar_t pathW[MAX_PATH] = {0};
    mbstowcs_s(nullptr, pathW, path, _TRUNCATE);

    return LoadLibraryEx(pathW, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}

void Library::unload(HMODULE library)
{
    FreeLibrary(library);
}

void* Library::getFunction(HMODULE library,
                           const char* name)
{
    return GetProcAddress(library, name);
}
#else
void* Library::load(const char* name)
{
    char path[MAX_PATH] = {0};
    getLoadedBinaryPath(name, path, MAX_PATH);

    return dlopen(path, RTLD_LAZY);
}

void Library::unload(void* library)
{
    dlclose(library);
}

void* Library::getFunction(void* library,
                           const char* name)
{
    return dlsym(library, name);
}
#endif

}

#if defined(IPL_OS_WINDOWS)
extern "C" FARPROC WINAPI delayLoadHook(unsigned int notify,
                                        DelayLoadInfo* dli)
{
    switch (notify)
    {
    case dliNotePreLoadLibrary:
        return reinterpret_cast<FARPROC>(ipl::Library::load(dli->szDll));
        break;

    default:
        break;
    }

    return NULL;
}
#endif
