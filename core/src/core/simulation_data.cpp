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

#include "simulation_data.h"

#include "energy_field_factory.h"
#include "impulse_response_factory.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SimulationData
// --------------------------------------------------------------------------------------------------------------------

SimulationData::SimulationData(bool enableIndirect,
                               bool enablePathing,
                               SceneType sceneType,
                               IndirectEffectType indirectType,
                               int maxNumOcclusionSamples,
                               float maxDuration,
                               int maxOrder,
                               int samplingRate,
                               int frameSize,
                               shared_ptr<OpenCLDevice> openCL,
                               shared_ptr<TANDevice> tan)
{
    directInputs.flags = static_cast<DirectSimulationFlags>(0);
    directInputs.occlusionType = OcclusionType::Raycast;
    directInputs.occlusionRadius = 0.0f;
    directInputs.numOcclusionSamples = maxNumOcclusionSamples;
    directInputs.numTransmissionRays = 1;

    reflectionInputs.enabled = false;
    pathingInputs.enabled = false;

    directOutputs.directPath.distanceAttenuation = 1.0f;
    directOutputs.directPath.airAbsorption[0] = 1.0f;
    directOutputs.directPath.airAbsorption[1] = 1.0f;
    directOutputs.directPath.airAbsorption[2] = 1.0f;
    directOutputs.directPath.directivity = 1.0f;
    directOutputs.directPath.occlusion = 1.0f;
    directOutputs.directPath.transmission[0] = 1.0f;
    directOutputs.directPath.transmission[1] = 1.0f;
    directOutputs.directPath.transmission[2] = 1.0f;

    if (enableIndirect)
    {
        reflectionInputs.reverbScale[0] = 1.0f;
        reflectionInputs.reverbScale[1] = 1.0f;
        reflectionInputs.reverbScale[2] = 1.0f;
        reflectionInputs.transitionTime = 1.0f;
        reflectionInputs.overlapFraction = 0.25f;
        reflectionInputs.baked = false;

        reflectionState.energyField = EnergyFieldFactory::create(sceneType, maxDuration, maxOrder, openCL);
        reflectionState.accumEnergyField = EnergyFieldFactory::create(sceneType, maxDuration, maxOrder, openCL);
        reflectionState.energyField->reset();
        reflectionState.accumEnergyField->reset();
        reflectionState.numFramesAccumulated = 0;

        if (indirectType != IndirectEffectType::Parametric)
        {
            reflectionState.impulseResponse = ImpulseResponseFactory::create(indirectType, maxDuration, maxOrder, samplingRate, openCL);
            reflectionState.impulseResponse->reset();

            reflectionState.impulseResponseCopy = ImpulseResponseFactory::create(indirectType, maxDuration, maxOrder, samplingRate, openCL);
            reflectionState.impulseResponseCopy->reset();

            auto numChannels = SphericalHarmonics::numCoeffsForOrder(maxOrder);
            auto irSize = static_cast<int>(ceilf(maxDuration * samplingRate));

            reflectionState.distanceAttenuationCorrectionCurve.resize(irSize);
            reflectionState.distanceAttenuationCorrectionCurve.zero();
            reflectionState.applyDistanceAttenuationCorrectionCurve = false;

            if (indirectType == IndirectEffectType::Convolution || indirectType == IndirectEffectType::Hybrid)
            {
                reflectionOutputs.overlapSaveFIR.initBuffers(numChannels, irSize, frameSize);
            }

            reflectionOutputs.numChannels = numChannels;
            reflectionOutputs.numSamples = irSize;
        }

        reflectionOutputs.reverb.reverbTimes[0] = .0f;
        reflectionOutputs.reverb.reverbTimes[1] = .0f;
        reflectionOutputs.reverb.reverbTimes[2] = .0f;

        if (indirectType == IndirectEffectType::Hybrid)
        {
            reflectionOutputs.hybridEQ[0] = 1.0f;
            reflectionOutputs.hybridEQ[1] = 1.0f;
            reflectionOutputs.hybridEQ[2] = 1.0f;
            reflectionOutputs.hybridDelay = static_cast<int>(ceilf((1.0f - reflectionInputs.overlapFraction) * reflectionInputs.transitionTime * samplingRate));
        }

#if defined(IPL_USES_TRUEAUDIONEXT)
        if (indirectType == IndirectEffectType::TrueAudioNext)
        {
            this->reflectionOutputs.tan = tan;
            reflectionOutputs.tanSlot = tan->acquireSlot();
        }
#endif
    }

    if (enablePathing)
    {
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            pathingState.eq[i] = 0.1f;
            pathingOutputs.eq[i] = 0.1f;
        }

        pathingState.sh.resize(SphericalHarmonics::numCoeffsForOrder(maxOrder));
        pathingOutputs.sh.resize(SphericalHarmonics::numCoeffsForOrder(maxOrder));
        pathingState.sh.zero();
        pathingOutputs.sh.zero();
    }
}

SimulationData::~SimulationData()
{
#if defined(IPL_USES_TRUEAUDIONEXT)
    if (reflectionOutputs.tan)
    {
        reflectionOutputs.tan->releaseSlot(reflectionOutputs.tanSlot);
    }
#endif
}

bool SimulationData::hasSourceChanged() const
{
    auto changed = ((reflectionInputs.source.origin - reflectionState.prevSource.origin).length() > 1e-4f);
    changed = changed || ((reflectionInputs.source.ahead - reflectionState.prevSource.ahead).length() > 1e-4f);
    changed = changed || ((reflectionInputs.source.up - reflectionState.prevSource.up).length() > 1e-4f);
    changed = changed || (fabsf(reflectionInputs.directivity.dipoleWeight - reflectionState.prevDirectivity.dipoleWeight) > 1e-4f);
    changed = changed || (fabsf(reflectionInputs.directivity.dipolePower - reflectionState.prevDirectivity.dipolePower) > 1e-4f);
    return changed;
}

}
