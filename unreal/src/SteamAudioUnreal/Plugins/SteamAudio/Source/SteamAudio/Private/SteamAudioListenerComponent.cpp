//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioListenerComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SteamAudioAudioEngineInterface.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioReverb.h"
#include "SteamAudioSettings.h"


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioListenerComponent
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioListenerComponent* USteamAudioListenerComponent::CurrentListener = nullptr;

USteamAudioListenerComponent::USteamAudioListenerComponent()
	: CurrentBakedListener(nullptr)
	, bSimulateReverb(false)
	, ReverbType(EReverbSimulationType::REALTIME)
	, Source(nullptr)
	, Simulator(nullptr)
	, PlayerController(nullptr)
{}

void USteamAudioListenerComponent::SetInputs()
{
	SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();
	if (!Manager.IsInitialized() || !Source)
		return;

    IPLSimulationInputs Inputs{};

    if (bSimulateReverb)
    {
        Inputs.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
    }

	if (PlayerController)
	{
		FVector ListenerPosition;
		FVector ListenerAhead;
		FVector ListenerUp;
		FVector ListenerRight;
		
		PlayerController->GetAudioListenerPosition(ListenerPosition, ListenerAhead, ListenerRight);
		ListenerUp = FVector::CrossProduct(ListenerRight, ListenerAhead);

        Inputs.source.origin = SteamAudio::ConvertVector(ListenerPosition);
        Inputs.source.ahead = SteamAudio::ConvertVector(ListenerAhead, false);
        Inputs.source.up = SteamAudio::ConvertVector(ListenerUp, false);
        Inputs.source.right = SteamAudio::ConvertVector(ListenerRight, false);
	}
	else
	{
		const FTransform& SourceTransform = GetOwner()->GetTransform();

		Inputs.source.origin = SteamAudio::ConvertVector(SourceTransform.GetLocation());
        Inputs.source.ahead = SteamAudio::ConvertVector(SourceTransform.GetUnitAxis(EAxis::X), false);
        Inputs.source.up = SteamAudio::ConvertVector(SourceTransform.GetUnitAxis(EAxis::Z), false);
        Inputs.source.right = SteamAudio::ConvertVector(SourceTransform.GetUnitAxis(EAxis::Y), false);
	}


    Inputs.reverbScale[0] = 1.0f;
    Inputs.reverbScale[1] = 1.0f;
    Inputs.reverbScale[2] = 1.0f;
    Inputs.hybridReverbTransitionTime = Manager.GetSteamAudioSettings().HybridReverbTransitionTime;
    Inputs.hybridReverbOverlapPercent = Manager.GetSteamAudioSettings().HybridReverbOverlapPercent / 100.0f;
    Inputs.baked = (ReverbType != EReverbSimulationType::REALTIME) ? IPL_TRUE : IPL_FALSE;

    Inputs.bakedDataIdentifier.type = IPL_BAKEDDATATYPE_REFLECTIONS;
    Inputs.bakedDataIdentifier.variation = IPL_BAKEDDATAVARIATION_REVERB;

    iplSourceSetInputs(Source, IPL_SIMULATIONFLAGS_REFLECTIONS, &Inputs);
}

IPLSimulationOutputs USteamAudioListenerComponent::GetOutputs()
{
	IPLSimulationOutputs Outputs{};

	if (Source)
    {
        iplSourceGetOutputs(Source, IPL_SIMULATIONFLAGS_REFLECTIONS, &Outputs);
    }

	return Outputs;
}

void USteamAudioListenerComponent::UpdateOutputs()
{}

IPLBakedDataIdentifier USteamAudioListenerComponent::GetBakedDataIdentifier() const
{
	IPLBakedDataIdentifier Identifier{};

	if (bSimulateReverb && ReverbType == EReverbSimulationType::BAKED)
	{
		Identifier.type = IPL_BAKEDDATATYPE_REFLECTIONS;
		Identifier.variation = IPL_BAKEDDATAVARIATION_REVERB;
	}

	return Identifier;
}

#if WITH_EDITOR
bool USteamAudioListenerComponent::CanEditChange(const FProperty* InProperty) const
{
	const bool bParentVal = Super::CanEditChange(InProperty);

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioListenerComponent, ReverbType))
		return bParentVal && bSimulateReverb;

	return bParentVal;
}
#endif

void USteamAudioListenerComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentListener = this;

	PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();
	if (!Manager.InitializeSteamAudio(SteamAudio::EManagerInitReason::PLAYING))
		return;

    Simulator = iplSimulatorRetain(Manager.GetSimulator());
	if (!Simulator)
		return;

    IPLSourceSettings SourceSettings{};
    SourceSettings.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;

    IPLerror Status = iplSourceCreate(Simulator, &SourceSettings, &Source);
	if (Status != IPL_STATUS_SUCCESS)
	{
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create source. [%d]"), Status);
		iplSimulatorRelease(&Simulator);
		return;
	}

    iplSourceAdd(Source, Simulator);
 
	Manager.AddListener(this);
    
	SteamAudio::IAudioEngineState* AudioEngineState = SteamAudio::FSteamAudioModule::GetAudioEngineState();
	if (AudioEngineState)
    {
        AudioEngineState->SetReverbSource(Source);
    }
}

void USteamAudioListenerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SteamAudio::IAudioEngineState* AudioEngineState = SteamAudio::FSteamAudioModule::GetAudioEngineState();
	if (AudioEngineState)
    {
        AudioEngineState->SetReverbSource(nullptr);
    }

    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();

	if (Simulator && Source)
	{
        Manager.RemoveListener(this);
        iplSourceRemove(Source, Simulator);
        iplSourceRelease(&Source);
        iplSimulatorRelease(&Simulator);
	}

	Super::EndPlay(EndPlayReason);
}

USteamAudioListenerComponent* USteamAudioListenerComponent::GetCurrentListener()
{
	return CurrentListener;
}
