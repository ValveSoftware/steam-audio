//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <stdexcept>

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#endif

#if defined(IPL_OS_MACOSX)
#include <mach-o/dyld.h>
#endif

#if (defined(IPL_OS_LINUX) || defined(IPL_OS_MACOSX) || defined(IPL_OS_ANDROID))
#include <dlfcn.h>
#endif

#include <fmod/fmod.hpp>

#include <phonon.h>
