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

#include "SteamAudioSourceComponent.h"
#include "SteamAudioAudioEngineInterface.h"
#include "SteamAudioBakedListenerComponent.h"
#include "SteamAudioBakedSourceComponent.h"
#include "SteamAudioCommon.h"
#include "SteamAudioListenerComponent.h"
#include "SteamAudioManager.h"
#include "SteamAudioProbeVolume.h"
#include "SteamAudioSettings.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSourceComponent
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioSourceComponent::USteamAudioSourceComponent()
    : bSimulateOcclusion(false)
    , OcclusionType(EOcclusionType::RAYCAST)
    , OcclusionRadius(1.0f)
    , OcclusionSamples(16)
    , OcclusionValue(1.0f)
    , bSimulateTransmission(false)
    , TransmissionLowValue(1.0f)
    , TransmissionMidValue(1.0f)
    , TransmissionHighValue(1.0f)
    , MaxTransmissionSurfaces(1)
    , bSimulateReflections(false)
    , ReflectionsType(EReflectionSimulationType::REALTIME)
    , CurrentBakedSource(nullptr)
    , bSimulatePathing(false)
    , PathingProbeBatch(nullptr)
    , bPathValidation(true)
    , bFindAlternatePaths(true)
    , Source(nullptr)
    , Simulator(nullptr)
    , AudioEngineSource(nullptr)
{
    bAutoActivate = true;
    PrimaryComponentTick.bCanEverTick = true;
}

void USteamAudioSourceComponent::SetInputs(IPLSimulationFlags Flags)
{
    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();
    if (!Manager.IsInitialized() || !Source)
        return;

    IPLSimulationInputs Inputs{};

    Inputs.flags = IPL_SIMULATIONFLAGS_DIRECT;
    if (bSimulateReflections)
    {
        Inputs.flags = static_cast<IPLSimulationFlags>(Inputs.flags | IPL_SIMULATIONFLAGS_REFLECTIONS);
    }
    if (bSimulatePathing && PathingProbeBatch.IsValid())
    {
        Inputs.flags = static_cast<IPLSimulationFlags>(Inputs.flags | IPL_SIMULATIONFLAGS_PATHING);
    }

    if (bSimulateOcclusion)
    {
        Inputs.directFlags = static_cast<IPLDirectSimulationFlags>(Inputs.directFlags | IPL_DIRECTSIMULATIONFLAGS_OCCLUSION);
        if (bSimulateTransmission)
        {
            Inputs.directFlags = static_cast<IPLDirectSimulationFlags>(Inputs.directFlags | IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION);
        }
    }

    const FTransform& SourceTransform = GetOwner()->GetTransform();
    Inputs.source.origin = SteamAudio::ConvertVector(SourceTransform.GetLocation());
    Inputs.source.ahead = SteamAudio::ConvertVector(SourceTransform.GetUnitAxis(EAxis::X), false);
    Inputs.source.up = SteamAudio::ConvertVector(SourceTransform.GetUnitAxis(EAxis::Z), false);
    Inputs.source.right = SteamAudio::ConvertVector(SourceTransform.GetUnitAxis(EAxis::Y), false);

    Inputs.occlusionType = static_cast<IPLOcclusionType>(OcclusionType);
    Inputs.occlusionRadius = OcclusionRadius;
    Inputs.numOcclusionSamples = OcclusionSamples;
    Inputs.numTransmissionRays = MaxTransmissionSurfaces;
    Inputs.reverbScale[0] = 1.0f;
    Inputs.reverbScale[1] = 1.0f;
    Inputs.reverbScale[2] = 1.0f;
    Inputs.hybridReverbTransitionTime = Manager.GetSteamAudioSettings().HybridReverbTransitionTime;
    Inputs.hybridReverbOverlapPercent = Manager.GetSteamAudioSettings().HybridReverbOverlapPercent / 100.0f;
    Inputs.baked = (ReflectionsType != EReflectionSimulationType::REALTIME) ? IPL_TRUE : IPL_FALSE;
    Inputs.visRadius = Manager.GetSteamAudioSettings().BakingVisibilityRadius;
    Inputs.visThreshold = Manager.GetSteamAudioSettings().BakingVisibilityThreshold;
    Inputs.visRange = Manager.GetSteamAudioSettings().BakingVisibilityRange;
    Inputs.pathingOrder = Manager.GetSteamAudioSettings().BakingAmbisonicOrder;
    Inputs.enableValidation = bPathValidation ? IPL_TRUE : IPL_FALSE;
    Inputs.findAlternatePaths = bFindAlternatePaths ? IPL_TRUE : IPL_FALSE;

    if (PathingProbeBatch)
    {
        Inputs.pathingProbes = PathingProbeBatch.Get()->GetProbeBatch();
    }

    Inputs.bakedDataIdentifier = GetBakedDataIdentifier();

    iplSourceSetInputs(Source, Flags, &Inputs);
}

IPLSimulationOutputs USteamAudioSourceComponent::GetOutputs(IPLSimulationFlags Flags)
{
    IPLSimulationOutputs Outputs{};

    if (Source)
    {
        iplSourceGetOutputs(Source, Flags, &Outputs);
    }

    return Outputs;
}

void USteamAudioSourceComponent::UpdateOutputs(IPLSimulationFlags Flags)
{
    IPLSimulationOutputs Outputs = GetOutputs(Flags);

    if (Flags & IPL_SIMULATIONFLAGS_DIRECT)
    {
        if (bSimulateOcclusion)
        {
            OcclusionValue = Outputs.direct.occlusion;
            if (bSimulateTransmission)
            {
                TransmissionLowValue = Outputs.direct.transmission[0];
                TransmissionMidValue = Outputs.direct.transmission[1];
                TransmissionHighValue = Outputs.direct.transmission[2];
            }
        }
    }
}

IPLBakedDataIdentifier USteamAudioSourceComponent::GetBakedDataIdentifier() const
{
    IPLBakedDataIdentifier Identifier{};

    if (bSimulatePathing)
    {
        Identifier.type = IPL_BAKEDDATATYPE_PATHING;
        Identifier.variation = IPL_BAKEDDATAVARIATION_DYNAMIC;
    }
    else if (bSimulateReflections && ReflectionsType != EReflectionSimulationType::REALTIME)
    {
        Identifier.type = IPL_BAKEDDATATYPE_REFLECTIONS;

        if (ReflectionsType == EReflectionSimulationType::BAKED_STATIC_SOURCE)
        {
            Identifier.variation = IPL_BAKEDDATAVARIATION_STATICSOURCE;

            if (CurrentBakedSource)
            {
                USteamAudioBakedSourceComponent* BakedSource = CurrentBakedSource.Get()->FindComponentByClass<USteamAudioBakedSourceComponent>();
                if (BakedSource)
                {
                    Identifier.endpointInfluence.center = SteamAudio::ConvertVector(BakedSource->GetOwner()->GetTransform().GetLocation());
                    Identifier.endpointInfluence.radius = BakedSource->InfluenceRadius;
                }
            }
        }
        else if (ReflectionsType == EReflectionSimulationType::BAKED_STATIC_LISTENER)
        {
            Identifier.variation = IPL_BAKEDDATAVARIATION_STATICLISTENER;

            USteamAudioListenerComponent* Listener = USteamAudioListenerComponent::GetCurrentListener();
            if (Listener)
            {
                if (Listener->CurrentBakedListener)
                {
                    USteamAudioBakedListenerComponent* BakedListener = Listener->CurrentBakedListener.Get()->FindComponentByClass<USteamAudioBakedListenerComponent>();
                    if (BakedListener)
                    {
                        Identifier.endpointInfluence.center = SteamAudio::ConvertVector(BakedListener->GetOwner()->GetTransform().GetLocation());
                        Identifier.endpointInfluence.radius = BakedListener->InfluenceRadius;
                    }
                }
            }
        }
    }

    return Identifier;
}

#if WITH_EDITOR
bool USteamAudioSourceComponent::CanEditChange(const FProperty* InProperty) const
{
    const bool bParentVal = Super::CanEditChange(InProperty);

    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, OcclusionType)))
        return bParentVal && bSimulateOcclusion;
    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, OcclusionRadius)))
        return bParentVal && bSimulateOcclusion && (OcclusionType == EOcclusionType::VOLUMETRIC);
    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, OcclusionSamples)))
        return bParentVal && bSimulateOcclusion && (OcclusionType == EOcclusionType::VOLUMETRIC);
    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, OcclusionValue)))
        return bParentVal && !bSimulateOcclusion;
    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, bSimulateTransmission)))
        return bParentVal && bSimulateOcclusion;
    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, TransmissionLowValue)))
        return bParentVal && !bSimulateTransmission;
    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, TransmissionMidValue)))
        return bParentVal && !bSimulateTransmission;
    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, TransmissionHighValue)))
        return bParentVal && !bSimulateTransmission;
	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, ReflectionsType)))
		return bParentVal && bSimulateReflections;
	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, CurrentBakedSource)))
		return bParentVal && bSimulateReflections && (ReflectionsType == EReflectionSimulationType::BAKED_STATIC_SOURCE);
	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, PathingProbeBatch)))
		return bParentVal && bSimulatePathing;
	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, bPathValidation)))
		return bParentVal && bSimulatePathing;
	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSourceComponent, bFindAlternatePaths)))
		return bParentVal && bSimulatePathing;

    return bParentVal;
}
#endif

void USteamAudioSourceComponent::BeginPlay()
{
    Super::BeginPlay();

    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();
    if (!Manager.InitializeSteamAudio(SteamAudio::EManagerInitReason::PLAYING))
        return;

    Simulator = iplSimulatorRetain(Manager.GetSimulator());
    if (!Simulator)
        return;

    IPLSourceSettings SourceSettings{};
    SourceSettings.flags = static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING);

    IPLerror Status = iplSourceCreate(Simulator, &SourceSettings, &Source);
    if (Status != IPL_STATUS_SUCCESS)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create source. [%d]"), Status);
        iplSimulatorRelease(&Simulator);
        return;
    }

    iplSourceAdd(Source, Simulator);
    Manager.AddSource(this);

    SteamAudio::IAudioEngineState* AudioEngineState = SteamAudio::FSteamAudioModule::GetAudioEngineState();
    if (AudioEngineState)
    {
        AudioEngineSource = AudioEngineState->CreateAudioEngineSource();
        if (AudioEngineSource)
        {
            AudioEngineSource->Initialize(GetOwner());
        }
    }
}

void USteamAudioSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AudioEngineSource)
    {
        AudioEngineSource->Destroy();
    }

    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();

    if (Simulator && Source)
    {
        Manager.RemoveSource(this);
        iplSourceRemove(Source, Simulator);
        iplSourceRelease(&Source);
        iplSimulatorRelease(&Simulator);
    }

    Super::EndPlay(EndPlayReason);
}

void USteamAudioSourceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    if (AudioEngineSource)
    {
        AudioEngineSource->UpdateParameters(this);
    }
}
