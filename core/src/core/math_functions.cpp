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

#include "math_functions.h"

#include <assert.h>
#include <cmath>

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Math
// --------------------------------------------------------------------------------------------------------------------

const double Math::kPiD = 3.14159265358979323846;
const float Math::kPi = 3.141592f;
const float Math::kDegreesToRadians = 0.017452f;
const float Math::kHalfPi = 1.570796f;

bool Math::isFinite(float x)
{
#if defined(IPL_OS_WINDOWS)
    return (_finite(x) != 0);
#elif (defined(IPL_OS_LINUX) || defined(IPL_OS_ANDROID))
    return finite(x);
#else
    return isfinite(x);
#endif
}

int Math::nextpow2(int x)
{
    assert(x > 0);

    uint32_t y = x - 1;
    y |= y >> 1;
    y |= y >> 2;
    y |= y >> 4;
    y |= y >> 8;
    y |= y >> 16;
    return static_cast<int>(y + 1);
}

uint64_t Math::factorial(uint32_t x)
{
    uint64_t result = 1;

    for (auto i = 2U; i <= x; ++i)
    {
        result *= i;
    }

    return result;
}

uint64_t Math::doubleFactorial(uint32_t x)
{
    uint64_t result = 1;
    auto start = x % 2 ? 1U : 2U;

    for (auto i = start; i <= x; i += 2)
    {
        result *= i;
    }

    return result;
}

}
