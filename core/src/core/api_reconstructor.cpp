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

#include "api_energy_field.h"
#include "api_impulse_response.h"
#include "api_reconstructor.h"

// --------------------------------------------------------------------------------------------------------------------
// CReconstructor
// --------------------------------------------------------------------------------------------------------------------

namespace api {

CReconstructor::CReconstructor(CContext* context, const IPLReconstructorSettings* settings)
{
    if (!context || !settings)
        throw Exception(Status::Failure);

    auto _context = context->mHandle.get();
    auto _maxDuration = settings->maxDuration;
    auto _maxOrder = settings->maxOrder;
    auto _samplingRate = settings->samplingRate;

    if (!_context)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<Reconstructor>(ipl::make_shared<Reconstructor>(_maxDuration, _maxOrder, _samplingRate), _context);
    new (&mTempEnergyFields) vector<const EnergyField*>();
    new (&mTempDistanceAttenuation) vector<const float*>();
    new (&mTempAirAbsorption) vector<AirAbsorptionModel*>();
    new (&mTempImpulseResponses) vector<ImpulseResponse*>();
}

CReconstructor* CReconstructor::retain()
{
    mHandle.retain();
    return this;
}

void CReconstructor::release()
{
    if (mHandle.release())
    {
        this->~CReconstructor();
        gMemory().free(this);
    }
}

void CReconstructor::reconstruct(IPLint32 numInputs, const IPLReconstructorInputs* inputs,
                                 const IPLReconstructorSharedInputs* sharedInputs, IPLReconstructorOutputs* outputs)
{
    if (!inputs || !sharedInputs || !outputs)
        return;

    mTempEnergyFields.resize(numInputs);
    mTempDistanceAttenuation.resize(numInputs);
    mTempAirAbsorption.resize(numInputs);
    mTempImpulseResponses.resize(numInputs);
    for (auto i = 0; i < numInputs; ++i)
    {
        if (!inputs[i].energyField || !outputs[i].impulseResponse)
            return;

        auto _energyField = static_cast<CEnergyField*>(reinterpret_cast<IEnergyField*>(inputs[i].energyField))->mHandle.get();
        auto _impulseResponse = static_cast<CImpulseResponse*>(reinterpret_cast<IImpulseResponse*>(outputs[i].impulseResponse))->mHandle.get();

        if (!_energyField || !_impulseResponse)
            return;

        mTempEnergyFields[i] = _energyField.get();
        mTempDistanceAttenuation[i] = nullptr;
        mTempAirAbsorption[i] = AirAbsorptionModel{};
        mTempImpulseResponses[i] = _impulseResponse.get();
    }

    auto _reconstructor = mHandle.get();
    auto _numIRs = numInputs;
    const auto* _energyFields = mTempEnergyFields.data();
    const auto* _distanceAttenuationCorrectionCurves = mTempDistanceAttenuation.data();
    const auto* _airAbsorptionModels = mTempAirAbsorption.data();
    const auto* _impulseResponses = mTempImpulseResponses.data();
    auto _reconstructionType = ReconstructionType::Linear;
    auto _duration = sharedInputs->duration;
    auto _order = sharedInputs->order;

    if (!_reconstructor)
        return;

    _reconstructor->reconstruct(_numIRs, _energyFields, _distanceAttenuationCorrectionCurves, _airAbsorptionModels,
                                _impulseResponses, _reconstructionType, _duration, _order);
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createReconstructor(const IPLReconstructorSettings* settings, IReconstructor** reconstructor)
{
    if (!settings || !reconstructor)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _reconstructor = reinterpret_cast<CReconstructor*>(gMemory().allocate(sizeof(CReconstructor), Memory::kDefaultAlignment));
        new (_reconstructor) CReconstructor(this, settings);
        *reconstructor = _reconstructor;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
