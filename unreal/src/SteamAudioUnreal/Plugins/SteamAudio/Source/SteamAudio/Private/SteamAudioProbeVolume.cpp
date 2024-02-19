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

#include "SteamAudioProbeVolume.h"
#include "Async/Async.h"
#include "Components/PrimitiveComponent.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioProbeComponent.h"
#include "SteamAudioScene.h"
#include "SteamAudioSerializedObject.h"
#include "SteamAudioStaticMeshActor.h"


// ---------------------------------------------------------------------------------------------------------------------
// ASteamAudioProbeVolume
// ---------------------------------------------------------------------------------------------------------------------

ASteamAudioProbeVolume::ASteamAudioProbeVolume()
	: Asset()
	, GenerationType(EProbeGenerationType::UNIFORM_FLOOR)
	, HorizontalSpacing(3.0f)
	, HeightAboveFloor(1.5f)
	, NumProbes(0)
	, DataSize(0)
	, Simulator(nullptr)
	, ProbeBatch(nullptr)
{
	UPrimitiveComponent* RootPrimitiveComponent = Cast<UPrimitiveComponent>(this->GetRootComponent());
	if (RootPrimitiveComponent)
    {
        RootPrimitiveComponent->BodyInstance.SetCollisionProfileName("NoCollision");
        RootPrimitiveComponent->SetGenerateOverlapEvents(false);
    }

	ProbeComponent = CreateDefaultSubobject<USteamAudioProbeComponent>(TEXT("ProbeComponent0"));
}

#if WITH_EDITOR
bool ASteamAudioProbeVolume::CanEditChange(const FProperty* InProperty) const
{
    const bool bParentVal = Super::CanEditChange(InProperty);

    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, HorizontalSpacing))
        return bParentVal && (GenerationType == EProbeGenerationType::UNIFORM_FLOOR);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, HeightAboveFloor))
        return bParentVal && (GenerationType == EProbeGenerationType::UNIFORM_FLOOR);

    return bParentVal;
}
#endif

void ASteamAudioProbeVolume::BeginPlay()
{
	Super::BeginPlay();

	SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();

	if (!Asset.IsAsset())
		return;

	// Make sure Steam Audio is initialized.
	if (!Manager.InitializeSteamAudio(SteamAudio::EManagerInitReason::PLAYING))
		return;

    Simulator = iplSimulatorRetain(Manager.GetSimulator());
	if (!Simulator)
		return;

    // Load the probe batch from the .uasset and add it to the simulator.
    ProbeBatch = SteamAudio::LoadProbeBatchFromAsset(Asset, Manager.GetContext());
	if (!ProbeBatch)
	{
		iplSimulatorRelease(&Simulator);
		return;
	}

    iplProbeBatchCommit(ProbeBatch);
    iplSimulatorAddProbeBatch(Simulator, ProbeBatch);
}

void ASteamAudioProbeVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Simulator && ProbeBatch)
	{
        iplSimulatorRemoveProbeBatch(Simulator, ProbeBatch);
        iplProbeBatchRelease(&ProbeBatch);
        iplSimulatorRelease(&Simulator);
	}

	Super::EndPlay(EndPlayReason);
}

bool ASteamAudioProbeVolume::GenerateProbes(ASteamAudioStaticMeshActor* StaticMeshActor, FString AssetName)
{
	check(StaticMeshActor);
    check(ProbeComponent);

	TPromise<bool> Promise;

	Async(EAsyncExecution::Thread, [this, StaticMeshActor, AssetName, &Promise]()
	{
        // Make sure Steam Audio is initialized.
		SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();
        bool bInitializeSucceeded = SteamAudio::RunInGameThread<bool>([&]()
        {
            return Manager.InitializeSteamAudio(SteamAudio::EManagerInitReason::GENERATING_PROBES);
        });
		if (!bInitializeSucceeded)
		{
			Promise.SetValue(false);
			return;
		}

		IPLContext Context = Manager.GetContext();
		IPLScene Scene = Manager.GetScene();

		// Load the static geometry data against which probes will be generated.
		IPLStaticMesh StaticMesh = SteamAudio::RunInGameThread<IPLStaticMesh>([&]()
		{
			return SteamAudio::LoadStaticMeshFromAsset(StaticMeshActor->Asset, Context, Scene);
		});
		if (!StaticMesh)
		{
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to load static mesh asset: %s"), *StaticMeshActor->Asset.GetAssetPathString());
			Manager.ShutDownSteamAudio();
            Promise.SetValue(false);
            return;
		}

        iplStaticMeshAdd(StaticMesh, Scene);
        iplSceneCommit(Scene);

        // Create a probe array and generate probes in it.
        IPLProbeArray ProbeArray = nullptr;
        IPLerror Status = iplProbeArrayCreate(Context, &ProbeArray);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create probe array. [%d]"), Status);
            iplStaticMeshRelease(&StaticMesh);
            Manager.ShutDownSteamAudio();
            Promise.SetValue(false);
            return;
        }

        FTransform Transform = GetTransform();
        Transform.MultiplyScale3D(FVector(2)); // todo: why?

        IPLProbeGenerationParams ProbeGenerationParams{};
        ProbeGenerationParams.type = static_cast<IPLProbeGenerationType>(GenerationType);
        ProbeGenerationParams.spacing = HorizontalSpacing;
        ProbeGenerationParams.height = HeightAboveFloor;
        ProbeGenerationParams.transform = SteamAudio::ConvertTransform(Transform, false);

        iplProbeArrayGenerateProbes(ProbeArray, Scene, &ProbeGenerationParams);

        // Create a probe batch and add the generated probes to it.
        IPLProbeBatch GeneratedProbeBatch = nullptr;
        Status = iplProbeBatchCreate(Context, &GeneratedProbeBatch);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create probe batch. [%d]"), Status);
            iplProbeArrayRelease(&ProbeArray);
            iplStaticMeshRelease(&StaticMesh);
            Manager.ShutDownSteamAudio();
            Promise.SetValue(false);
            return;
        }

        iplProbeBatchAddProbeArray(GeneratedProbeBatch, ProbeArray);

        IPLSerializedObjectSettings SerializedObjectSettings{};

        // Save the probe batch to a .uasset file.
        IPLSerializedObject SerializedObject = nullptr;
        Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create serialized object. [%d]"), Status);
            iplProbeBatchRelease(&GeneratedProbeBatch);
            iplProbeArrayRelease(&ProbeArray);
            iplStaticMeshRelease(&StaticMesh);
            Manager.ShutDownSteamAudio();
            Promise.SetValue(false);
            return;
        }

        iplProbeBatchSave(GeneratedProbeBatch, SerializedObject);

        USteamAudioSerializedObject* AssetObject = SteamAudio::RunInGameThread<USteamAudioSerializedObject*>([&]()
        {
            return USteamAudioSerializedObject::SerializeObjectToPackage(SerializedObject, AssetName);
        });
        if (!AssetObject)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to serialize probe batch."));
            iplSerializedObjectRelease(&SerializedObject);
            iplProbeBatchRelease(&GeneratedProbeBatch);
            iplProbeArrayRelease(&ProbeArray);
            iplStaticMeshRelease(&StaticMesh);
            Manager.ShutDownSteamAudio();
            Promise.SetValue(false);
            return;
        }

        SteamAudio::RunInGameThread<void>([&]()
        {
            // Update stats.
            Asset = AssetObject;
            NumProbes = iplProbeArrayGetNumProbes(ProbeArray);
            UpdateTotalSize(iplSerializedObjectGetSize(SerializedObject));
            ResetLayers();

            // Update probe positions for visualization.
            {
                FScopeLock Lock(&ProbeComponent->ProbePositionsCriticalSection);

                TArray<FVector>& ProbePositions = ProbeComponent->ProbePositions;
                ProbePositions.Empty();
                ProbePositions.SetNumUninitialized(NumProbes);

                for (int i = 0; i < NumProbes; ++i)
                {
                    ProbePositions[i] = SteamAudio::ConvertVectorInverse(iplProbeArrayGetProbe(ProbeArray, i).center);
                }
            }

            MarkPackageDirty();
        });

        iplSerializedObjectRelease(&SerializedObject);
        iplProbeBatchRelease(&GeneratedProbeBatch);
        iplProbeArrayRelease(&ProbeArray);
        iplStaticMeshRelease(&StaticMesh);
		Manager.ShutDownSteamAudio();
		Promise.SetValue(true);
	});

	TFuture<bool> Value = Promise.GetFuture();
	Value.Wait();

	return Value.Get();
}

void ASteamAudioProbeVolume::UpdateTotalSize(int Size)
{
	DataSize = Size;
}

void ASteamAudioProbeVolume::ResetLayers()
{
	DetailedStats.Empty();
}

void ASteamAudioProbeVolume::RemoveLayer(const FString& Name)
{
	int Index = FindLayer(Name);
	if (Index != INDEX_NONE)
	{
		DetailedStats.RemoveAt(Index);
	}
}

void ASteamAudioProbeVolume::AddOrUpdateLayer(const FString& Name, IPLBakedDataIdentifier& Identifier, int Size)
{
	int Index = FindLayer(Name);
	if (Index == INDEX_NONE)
	{
		AddLayer(Name, Identifier, Size);
	}
	else
	{
		UpdateLayer(Name, Size);
	}
}

void ASteamAudioProbeVolume::AddLayer(const FString& Name, IPLBakedDataIdentifier& Identifier, int Size)
{
	FSteamAudioBakedDataInfo Info;
	Info.Name = Name;
	Info.Type = static_cast<int>(Identifier.type);
	Info.Variation = static_cast<int>(Identifier.variation);
	Info.EndpointCenter = SteamAudio::ConvertVectorInverse(Identifier.endpointInfluence.center);
	Info.EndpointRadius = Identifier.endpointInfluence.radius;
	Info.Size = Size;

	DetailedStats.Add(Info);
}

void ASteamAudioProbeVolume::UpdateLayer(const FString& Name, int Size)
{
	int Index = FindLayer(Name);
	if (Index != INDEX_NONE)
	{
		DetailedStats[Index].Size = Size;
	}
}

int ASteamAudioProbeVolume::FindLayer(const FString& Name)
{
	return DetailedStats.IndexOfByPredicate([&Name](const FSteamAudioBakedDataInfo& Info) { return Info.Name == Name; });
}
