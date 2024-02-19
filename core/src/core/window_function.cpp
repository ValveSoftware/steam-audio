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

#include "window_function.h"

#include "math_functions.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// WindowFunction
// --------------------------------------------------------------------------------------------------------------------

void WindowFunction::rectangular(int size,
                                 float* window)
{
    for (auto i = 0; i < size; ++i)
    {
        window[i] = 1.0f;
    }
}

void WindowFunction::bartlett(int size,
                              float* window)
{
    for (auto i = 0; i < size; ++i)
    {
        window[i] = 1.0f - fabsf((i - (size - 1) / 2.0f) / ((size - 1) / 2.0f));
    }
}

void WindowFunction::hann(int size,
                          float* window)
{
    auto m = size - 1;
    for (auto i = 0; i < size; ++i)
    {
        window[i] = 0.5f * (1.0f - cosf((2.0f * Math::kPi * i) / m));
    }
}

void WindowFunction::hamming(int size,
                             float* window)
{
    auto m = size - 1;
    for (auto i = 0; i < size; ++i)
    {
        window[i] = 0.54f - 0.46f * cosf((2.0f * Math::kPi * i) / m);
    }
}

void WindowFunction::blackman(int size,
                              float* window)
{
    auto m = size - 1;
    for (auto i = 0; i < size; ++i)
    {
        window[i] = 0.42f - 0.5f * cosf((2.0f * Math::kPi * i) / m) + 0.08f * cosf((4.0f * Math::kPi * i) / m);
    }
}

void WindowFunction::blackmanHarris(int size,
                                    float* window)
{
    auto m = size - 1;
    for (auto i = 0; i < size; ++i)
    {
        window[i] = 0.35875f - 0.48829f * cosf((2.0f * Math::kPi * i) / m) + 0.14128f * cosf((4.0f * Math::kPi * i) / m) - 0.01168f * cosf((6.0f * Math::kPi * i) / m);
    }
}

void WindowFunction::tukey(int size,
                           int overlapSize,
                           float* window)
{
    auto k = overlapSize - 1;
    auto n = size + overlapSize;

    for (auto i = 0; i < overlapSize; ++i)
    {
        window[i] = 0.5f * (1.0f + cosf(Math::kPi * (static_cast<float>(i) / static_cast<float>(k) - 1.0f)));
    }

    for (auto i = overlapSize; i < size; ++i)
    {
        window[i] = 1.0f;
    }

    for (auto i = size; i < n; ++i)
    {
        window[i] = 0.5f * (1.0f + cosf(Math::kPi * (1.0f - static_cast<float>(n - i - 1) / static_cast<float>(k))));
    }
}

}