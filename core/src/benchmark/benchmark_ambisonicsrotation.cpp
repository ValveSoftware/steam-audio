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

#include <profiler.h>
#include <sh.h>
using namespace ipl;

#include <phonon.h>

#include "phonon_perf.h"

BENCHMARK(ambisonicsrotation)
{
    PrintOutput("Running benchmark: Ambisonics Rotation...\n");

    auto order = 3;
    auto numRuns = 100000;
    auto frameSize = 1024;

    Quaternionf quaternion(1.0f, 0.0f, 0.0f, 2.0f);
    SHRotation rotation(order);

    Timer timer;
    timer.start();
    for (auto i = 0; i < numRuns; ++i)
        rotation.setRotation(quaternion);
    auto timeElapsed = timer.elapsedMicroseconds() / numRuns;

    PrintOutput("Create: %.2f us\n", timeElapsed);

    vector<float> coeffs(SphericalHarmonics::numCoeffsForOrder(order));
    vector<float> rotatedCoeffs(SphericalHarmonics::numCoeffsForOrder(order));

    Timer timer2;
    timer2.start();
    for (auto i = 0; i < numRuns; ++i)
        rotation.apply(order, coeffs.data(), rotatedCoeffs.data());
    timeElapsed = timer2.elapsedMilliseconds() / numRuns;

    PrintOutput("Apply:  %.2f ms\n", timeElapsed * frameSize);

    PrintOutput("\n");
}