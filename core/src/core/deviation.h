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
// DeviationModel
// --------------------------------------------------------------------------------------------------------------------

typedef float (IPL_CALLBACK *DeviationCallback)(float angle, int band, void* userData);

class DeviationModel
{
public:
    DeviationModel();

    DeviationModel(DeviationCallback callback, void* userData);

    bool isDefault() const;

    float evaluate(float angle, int band) const;

private:
    DeviationCallback mCallback;
    void* mUserData;

    static float utdDeviation(float angle, int band);
};

}
