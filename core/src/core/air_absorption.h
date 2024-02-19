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

#include "bands.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AirAbsorptionModel
// --------------------------------------------------------------------------------------------------------------------

typedef float (IPL_CALLBACK *AirAbsorptionCallback)(float distance,
                                                    int band,
                                                    void* userData);

class AirAbsorptionModel
{
public:
    float coefficients[Bands::kNumBands];
    AirAbsorptionCallback callback;
    void* userData;

    AirAbsorptionModel();

    AirAbsorptionModel(const float* coefficients,
                       AirAbsorptionCallback callback,
                       void* userData);

    bool isDefault() const;

    float evaluate(float distance,
                   int band) const;

private:
    static const float kDefaultCoefficients[Bands::kNumBands];
};

bool operator==(const AirAbsorptionModel& lhs,
                const AirAbsorptionModel& rhs);

bool operator!=(const AirAbsorptionModel& lhs,
                const AirAbsorptionModel& rhs);
}
