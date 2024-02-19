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

#include "types.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Math
// --------------------------------------------------------------------------------------------------------------------

// Various mathematical constants and standard functions.
namespace Math
{
    extern const double kPiD;
    extern const float kPi;
    extern const float kDegreesToRadians;
    extern const float kHalfPi;

    bool isFinite(float x);

    int nextpow2(int x);

    // Computes the product of all integers in the range [1, x].
    uint64_t factorial(uint32_t x);

    // Computes the product of all integers in the range [1, x] that have the same parity as x.
    uint64_t doubleFactorial(uint32_t x);

    template <typename T>
    T clamp(const T& x, const T& lower, const T& upper)
    {
        return std::max(lower, std::min(x, upper));
    }
};

}