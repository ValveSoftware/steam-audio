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

#include "air_absorption.h"
#include "propagation_medium.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AirAbsorptionModel
// --------------------------------------------------------------------------------------------------------------------

#if defined(IPL_ENABLE_OCTAVE_BANDS)
const float AirAbsorptionModel::kDefaultCoefficients[Bands::kNumBands] = {
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.00011513f,
    0.00034539f,
    0.00057565f,
    0.0011513f,
    0.0034539f,
    0.012089f,
    0.041907f,
};
#else
const float AirAbsorptionModel::kDefaultCoefficients[Bands::kNumBands] = { 0.0002f, 0.0017f, 0.0182f };
#endif

AirAbsorptionModel::AirAbsorptionModel()
    : callback(nullptr)
    , userData(nullptr)
{
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        coefficients[i] = kDefaultCoefficients[i];
    }
}

AirAbsorptionModel::AirAbsorptionModel(const float* coefficients,
                                       AirAbsorptionCallback callback,
                                       void* userData)
    : callback(callback)
    , userData(userData)
{
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        this->coefficients[i] = coefficients[i];
    }
}

bool AirAbsorptionModel::isDefault() const
{
    return (memcmp(coefficients, kDefaultCoefficients, Bands::kNumBands * sizeof(float)) == 0 &&
            callback == nullptr);
}

float AirAbsorptionModel::evaluate(float distance,
                                   int band) const
{
    if (callback)
    {
        return callback(distance, band, userData);
    }
    else
    {
        return expf(-coefficients[band] * distance);
    }
}

bool operator==(const AirAbsorptionModel& lhs,
                const AirAbsorptionModel& rhs)
{
    return (memcmp(lhs.coefficients, rhs.coefficients, Bands::kNumBands * sizeof(float)) == 0 &&
            lhs.callback == rhs.callback &&
            lhs.userData == rhs.userData);
}

bool operator!=(const AirAbsorptionModel& lhs,
                const AirAbsorptionModel& rhs)
{
    return !(lhs == rhs);
}

}
