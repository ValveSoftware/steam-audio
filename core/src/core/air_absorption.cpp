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

const float AirAbsorptionModel::kDefaultCoefficients[Bands::kNumBands] = { 0.0002f, 0.0017f, 0.0182f };

AirAbsorptionModel::AirAbsorptionModel()
    : callback(nullptr)
    , userData(nullptr)
{
    coefficients[0] = kDefaultCoefficients[0];
    coefficients[1] = kDefaultCoefficients[1];
    coefficients[2] = kDefaultCoefficients[2];
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
