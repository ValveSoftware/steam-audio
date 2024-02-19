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

#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Timer
// --------------------------------------------------------------------------------------------------------------------

void Timer::start()
{
    mStartTime = std::chrono::high_resolution_clock::now();
}

double Timer::elapsedSeconds() const
{
    std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - mStartTime;
    return duration.count();
}

double Timer::elapsedMilliseconds() const
{
    std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - mStartTime;
    return duration.count();
}

double Timer::elapsedMicroseconds() const
{
    std::chrono::duration<double, std::micro> duration = std::chrono::high_resolution_clock::now() - mStartTime;
    return duration.count();
}

}
