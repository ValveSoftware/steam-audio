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

#include "memory_allocator.h"
#include <chrono>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Timer
// --------------------------------------------------------------------------------------------------------------------

// Allows high-precision timing using the platform's most precise timer.
class Timer
{
public:
    // Records the current timer value as the "start" time.
    void start();

    // Returns the amount of time elapsed since the "start" time.
    double elapsedSeconds() const;
    double elapsedMilliseconds() const;
    double elapsedMicroseconds() const;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> mStartTime;
};


// --------------------------------------------------------------------------------------------------------------------
// Profiler
// --------------------------------------------------------------------------------------------------------------------

namespace Profiler
{
    // Passes the main app's Telemetry API pointer, so profile information from Steam Audio can be integrated into the
    // profile information for the main app. See "Profiling a DLL" in the Telemetry 3 docs for more.
    void setProfilerContext(void* profilerContext);
}

}

#if defined(IPL_ENABLE_TELEMETRY)
#include "telemetry_profiler.h"
#elif defined(IPL_ENABLE_TRACY)
#include "tracy_profiler.h"
#else
#include "null_profiler.h"
#endif
