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

#include "energy_field_factory.h"
#include "impulse_response_factory.h"
#include "opencl_energy_field.h"
#include "opencl_impulse_response.h"
#include "reconstructor_factory.h"
#include "reflection_simulator_factory.h"
#include "simulation_manager.h"
#include "sh.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

//
// DISTANCE ATTENUATION
//

IPLfloat32 CContext::calculateDistanceAttenuation(IPLVector3 source,
                                                  IPLVector3 listener,
                                                  IPLDistanceAttenuationModel* model)
{
    if (!model)
        return 1.0f;

    auto _source = *reinterpret_cast<Vector3f*>(&source);
    auto _listener = *reinterpret_cast<Vector3f*>(&listener);

    DistanceAttenuationModel _model{};
    switch (model->type)
    {
    case IPL_DISTANCEATTENUATIONTYPE_DEFAULT:
        _model = DistanceAttenuationModel{};
        break;
    case IPL_DISTANCEATTENUATIONTYPE_INVERSEDISTANCE:
        _model = DistanceAttenuationModel(model->minDistance, nullptr, nullptr);
        break;
    case IPL_DISTANCEATTENUATIONTYPE_CALLBACK:
        _model = DistanceAttenuationModel(1.0f, model->callback, model->userData);
        break;
    }

    return _model.evaluate((_source - _listener).length());
}

//
// AIR ABSORPTION
//

void CContext::calculateAirAbsorption(IPLVector3 source,
                                      IPLVector3 listener,
                                      IPLAirAbsorptionModel* model,
                                      IPLfloat32* airAbsorption)
{
    if (!model || !airAbsorption)
        return;

    auto _source = *reinterpret_cast<Vector3f*>(&source);
    auto _listener = *reinterpret_cast<Vector3f*>(&listener);

    AirAbsorptionModel _model{};
    switch (model->type)
    {
    case IPL_AIRABSORPTIONTYPE_DEFAULT:
        _model = AirAbsorptionModel{};
        break;
    case IPL_AIRABSORPTIONTYPE_EXPONENTIAL:
        _model = AirAbsorptionModel(model->coefficients, nullptr, nullptr);
        break;
    case IPL_AIRABSORPTIONTYPE_CALLBACK:
        _model = AirAbsorptionModel(nullptr, model->callback, model->userData);
        break;
    }

    auto distance = (_source - _listener).length();

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        airAbsorption[i] = _model.evaluate(distance, i);
    }
}

//
// DIRECTIVITY
//

IPLfloat32 CContext::calculateDirectivity(IPLCoordinateSpace3 source,
                                          IPLVector3 listener,
                                          IPLDirectivity* model)
{
    if (!model)
        return 1.0f;

    auto _source = *reinterpret_cast<CoordinateSpace3f*>(&source);
    auto _listener = *reinterpret_cast<Vector3f*>(&listener);
    auto _directivity = *reinterpret_cast<Directivity*>(model);

    return _directivity.evaluateAt(_listener, _source);
}

}
