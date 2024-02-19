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

// Functions for element-wise mathematical operations on arrays of real- or complex-valued numbers. SIMD-accelerated
// where possible.
namespace ArrayMath
{
    // Real-valued addition.
    void add(int size,
             const float* in1,
             const float* in2,
             float* out);

    // Complex-valued addition.
    void add(int size,
             const complex_t* in1,
             const complex_t* in2,
             complex_t* out);

    // Real-valued multiplication.
    void multiply(int size,
                  const float* in1,
                  const float* in2,
                  float* out);

    // Complex-valued multiplication.
    void multiply(int size,
                  const complex_t* in1,
                  const complex_t* in2,
                  complex_t* out);

    // Real-valued multiply-accumulate.
    void multiplyAccumulate(int size,
                            float const* in1,
                            float const* in2,
                            float* accum);

    // Complex-valued multiply-accumulate.
    void multiplyAccumulate(int size,
                            const complex_t* in1,
                            const complex_t* in2,
                            complex_t* accum);

    // Scaling by a constant.
    void scale(int size,
               const float* in,
               float scalar,
               float* out);

    // Scaling by a constant.
    void scale(int size,
               const complex_t* in,
               float scalar,
               complex_t* out);

    // Scale by a constant and accumulate.
    void scaleAccumulate(int size,
                         const float* in,
                         float scalar,
                         float* out);

    // Addition with a constant.
    void addConstant(int size,
                     const float* in,
                     float constant,
                     float* out);

    // Find the max value in the array.
    void max(int size,
             const float* in,
             float& out);

    // Find the index of the max value in the array.
    void maxIndex(int size,
                  const float* in,
                  float& outValue,
                  int& outIndex);

    // Thresholds all elements in the array to be greater than or equal to the specified minimum value.
    void threshold(int size,
                   const float* in,
                   float minValue,
                   float* out);

    // Natural logarithm.
    void log(int size,
             const float* in,
             float* out);

    // Real-valued exponential function.
    void exp(int size,
             const float* in,
             float* out);

    // Complex-valued exponential function.
    void exp(int size,
             const complex_t* in,
             complex_t* out);

    // Calculates the magnitude of each complex number.
    void magnitude(int size,
                   const complex_t* in,
                   float* out);

    // Calculates the phase of each complex number.
    void phase(int size,
               const complex_t* in,
               float* out);

    // Calculates complex numbers given an array of magnitudes and an array of phases.
    void polarToCartesian(int size,
                          const float* inMagnitude,
                          const float* inPhase,
                          complex_t* out);
}

}
