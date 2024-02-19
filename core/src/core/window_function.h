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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// WindowFunction
// --------------------------------------------------------------------------------------------------------------------

// Functions for generating various standard window functions.
namespace WindowFunction
{
    // Rectangular window.
    void rectangular(int size,
                     float* window);

    // Bartlett (triangular) window.
    void bartlett(int size,
                  float* window);

    // Hann window.
    void hann(int size,
              float* window);

    // Hamming window.
    void hamming(int size,
                 float* window);

    // Blackman window.
    void blackman(int size,
                  float* window);

    // Blackman-Harris window.
    void blackmanHarris(int size,
                        float* window);

    // Tukey window.
    void tukey(int size,
               int overlapSize,
               float* window);
};

}