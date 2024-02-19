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

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#else
#if defined(IPL_OS_MACOSX)
#include <mach-o/dyld.h>
#endif
#include <dlfcn.h>
#endif

#include <phonon.h>

namespace SteamAudioFMOD {

namespace Library
{
    void getLoadingBinaryPath(char* loadingBinaryPath,
                              int maxPathLength);

    void getLoadedBinaryPath(const char* name,
                             char* loadedBinaryPath,
                             int maxPathLength);

#if defined(IPL_OS_WINDOWS)
    HMODULE load(const char* name);

    void unload(HMODULE library);

    void* getFunction(HMODULE library,
                      const char* name);
#else
    void* load(const char* name);

    void unload(void* library);

    void* getFunction(void* library,
                      const char* name);
#endif
}

#define DECLARE_LIBRARY_FUNCTION(x) decltype(x)* fn_##x
#define DYNAMIC_LINK_LIBRARY_FUNCTION(x) this->fn_##x = reinterpret_cast<decltype(x)*>(Library::getFunction(library, #x))
#define IPL_API(x) gAPI().fn_##x

struct API
{
#if defined(IPL_OS_WINDOWS)
    HMODULE library;
#else
    void* library;
#endif

    DECLARE_LIBRARY_FUNCTION(iplContextCreate);

    API();

    ~API();
};

const API& gAPI();

}
