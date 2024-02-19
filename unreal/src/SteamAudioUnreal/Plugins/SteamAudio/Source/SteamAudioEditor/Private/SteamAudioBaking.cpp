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

#include "SteamAudioBaking.h"
#include "EngineUtils.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "SteamAudioBakedListenerComponent.h"
#include "SteamAudioBakedSourceComponent.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioProbeVolume.h"
#include "SteamAudioScene.h"
#include "SteamAudioSerializedObject.h"
#include "SteamAudioSettings.h"
#include "SteamAudioStaticMeshActor.h"
#include "TickableNotification.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FBakeTask
// ---------------------------------------------------------------------------------------------------------------------

FString FBakeTask::GetLayerName() const
{
    if (Type == EBakeTaskType::REVERB)
        return "Reverb";
    else if (Type == EBakeTaskType::STATIC_SOURCE_REFLECTIONS)
        return BakedSource->GetOwner()->GetName();
    else if (Type == EBakeTaskType::STATIC_LISTENER_REFLECTIONS)
        return BakedListener->GetOwner()->GetName();
    else if (Type == EBakeTaskType::PATHING)
        return PathingProbeVolume->GetName();
    else
        return "(unknown)";
}


// ---------------------------------------------------------------------------------------------------------------------
// Baking
// ---------------------------------------------------------------------------------------------------------------------

std::atomic<bool> GIsBaking(false);
static int GNumBakeTasks = 0;
static int GCurrentBakeTask = 0;
static int GNumProbeVolumes = 0;
static int GCurrentProbeVolume = 0;

static void CancelBake()
{
    IPLContext Context = SteamAudio::FSteamAudioModule::GetManager().GetContext();

    iplReflectionsBakerCancelBake(Context);
    iplPathBakerCancelBake(Context);

    GIsBaking = false;
}

static void STDCALL BakeProgressCallback(float Progress, void* UserData)
{
    FSteamAudioEditorModule::NotifyUpdate(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "BakeProgress", "Probe Volume {0}/{1}\nTask {2}/{3}\nBaking ({4})..."),
        FText::AsNumber(GCurrentProbeVolume), FText::AsNumber(GNumProbeVolumes),
        FText::AsNumber(GCurrentBakeTask), FText::AsNumber(GNumBakeTasks),
        FText::AsPercent(Progress)));
}

static EBakeResult BakeInternal(ASteamAudioStaticMeshActor* StaticMeshActor, const TArray<AActor*>& ProbeVolumes, const TArray<FBakeTask>& Tasks)
{
    TPromise<int> Promise;

    Async(EAsyncExecution::Thread, [StaticMeshActor, ProbeVolumes, Tasks, &Promise]()
    {
        int NumBakesSucceeded = 0;

		SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();
        bool bInitializeSucceeded = SteamAudio::RunInGameThread<bool>([&]()
        {
            return Manager.InitializeSteamAudio(SteamAudio::EManagerInitReason::BAKING);
        });
        if (!bInitializeSucceeded)
        {
            Promise.SetValue(0);
            return;
        }

		IPLContext Context = Manager.GetContext();
		IPLScene Scene = Manager.GetScene();

        IPLStaticMesh StaticMesh = SteamAudio::RunInGameThread<IPLStaticMesh>([&]()
        {
            return SteamAudio::LoadStaticMeshFromAsset(StaticMeshActor->Asset, Context, Scene);
        });
        if (!StaticMesh)
        {
            UE_LOG(LogSteamAudioEditor, Error, TEXT("Unable to load static mesh asset: %s"), *StaticMeshActor->Asset.GetAssetPathString());
            Manager.ShutDownSteamAudio();
            Promise.SetValue(0);
            return;
        }

        iplStaticMeshAdd(StaticMesh, Scene);
        iplSceneCommit(Scene);

        IPLSimulationSettings SimulationSettings = Manager.GetBakingSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

        IPLReflectionsBakeParams ReflectionsBakeParams{};
        ReflectionsBakeParams.scene = Scene;
        ReflectionsBakeParams.sceneType = SimulationSettings.sceneType;
        ReflectionsBakeParams.numRays = SimulationSettings.maxNumRays;
        ReflectionsBakeParams.numDiffuseSamples = SimulationSettings.numDiffuseSamples;
        ReflectionsBakeParams.numBounces = GetDefault<USteamAudioSettings>()->BakingBounces;
        ReflectionsBakeParams.simulatedDuration = SimulationSettings.maxDuration;
        ReflectionsBakeParams.savedDuration = SimulationSettings.maxDuration;
        ReflectionsBakeParams.order = SimulationSettings.maxOrder;
        ReflectionsBakeParams.numThreads = GetNumThreadsForCPUCoresPercentage(GetDefault<USteamAudioSettings>()->BakingCPUCoresPercentage);
        ReflectionsBakeParams.rayBatchSize = 1;
        ReflectionsBakeParams.irradianceMinDistance = GetDefault<USteamAudioSettings>()->BakingIrradianceMinDistance;
        ReflectionsBakeParams.bakeBatchSize = (SimulationSettings.sceneType == IPL_SCENETYPE_RADEONRAYS) ? GetDefault<USteamAudioSettings>()->BakingBatchSize : 1;
        ReflectionsBakeParams.openCLDevice = SimulationSettings.openCLDevice;
        ReflectionsBakeParams.radeonRaysDevice = SimulationSettings.radeonRaysDevice;

        if (GetDefault<USteamAudioSettings>()->bBakeConvolution)
        {
            ReflectionsBakeParams.bakeFlags = static_cast<IPLReflectionsBakeFlags>(ReflectionsBakeParams.bakeFlags | IPL_REFLECTIONSBAKEFLAGS_BAKECONVOLUTION);
        }
        if (GetDefault<USteamAudioSettings>()->bBakeParametric)
        {
            ReflectionsBakeParams.bakeFlags = static_cast<IPLReflectionsBakeFlags>(ReflectionsBakeParams.bakeFlags | IPL_REFLECTIONSBAKEFLAGS_BAKEPARAMETRIC);
        }

        IPLPathBakeParams PathBakeParams{};
        PathBakeParams.scene = Scene;
        PathBakeParams.numSamples = SimulationSettings.numVisSamples;
        PathBakeParams.radius = GetDefault<USteamAudioSettings>()->BakingVisibilityRadius;
        PathBakeParams.threshold = GetDefault<USteamAudioSettings>()->BakingVisibilityThreshold;
        PathBakeParams.visRange = GetDefault<USteamAudioSettings>()->BakingVisibilityRange;
        PathBakeParams.pathRange = GetDefault<USteamAudioSettings>()->BakingPathRange;
        PathBakeParams.numThreads = GetNumThreadsForCPUCoresPercentage(GetDefault<USteamAudioSettings>()->BakedPathingCPUCoresPercentage);

        for (AActor* Actor : ProbeVolumes)
        {
            ASteamAudioProbeVolume* ProbeVolume = Cast<ASteamAudioProbeVolume>(Actor);
            if (!ProbeVolume || !ProbeVolume->Asset.IsValid())
            {
                UE_LOG(LogSteamAudioEditor, Warning, TEXT("No probes generated in probe volume, skipping."));
                GCurrentProbeVolume++;
                continue;
            }

            IPLProbeBatch ProbeBatch = SteamAudio::RunInGameThread<IPLProbeBatch>([&]()
            {
                return SteamAudio::LoadProbeBatchFromAsset(ProbeVolume->Asset, Context);
            });
            if (!ProbeBatch)
            {
                UE_LOG(LogSteamAudioEditor, Warning, TEXT("Unable to load probe batch: %s"), *ProbeVolume->Asset.GetAssetPathString());
                GCurrentProbeVolume++;
                continue;
            }

            ReflectionsBakeParams.probeBatch = ProbeBatch;
            PathBakeParams.probeBatch = ProbeBatch;

            int NumBakesSucceededForProbeBatch = 0;

            for (const FBakeTask& Task : Tasks)
            {
                if (Task.Type == EBakeTaskType::PATHING && Task.PathingProbeVolume != ProbeVolume)
                {
                    GCurrentBakeTask++;
                    continue;
                }

                if (Task.Type == EBakeTaskType::PATHING)
                {
                    PathBakeParams.identifier.type = IPL_BAKEDDATATYPE_PATHING;
                    PathBakeParams.identifier.variation = IPL_BAKEDDATAVARIATION_DYNAMIC;
                }
                else
                {
                    ReflectionsBakeParams.identifier.type = IPL_BAKEDDATATYPE_REFLECTIONS;
                    if (Task.Type == EBakeTaskType::STATIC_SOURCE_REFLECTIONS)
                    {
                        ReflectionsBakeParams.identifier.variation = IPL_BAKEDDATAVARIATION_STATICSOURCE;

                        if (Task.BakedSource)
                        {
                            ReflectionsBakeParams.identifier.endpointInfluence.center = SteamAudio::ConvertVector(Task.BakedSource->GetOwner()->GetTransform().GetLocation());
                            ReflectionsBakeParams.identifier.endpointInfluence.radius = Task.BakedSource->InfluenceRadius;
                        }
                    }
                    else if (Task.Type == EBakeTaskType::STATIC_LISTENER_REFLECTIONS)
                    {
                        ReflectionsBakeParams.identifier.variation = IPL_BAKEDDATAVARIATION_STATICLISTENER;

                        if (Task.BakedListener)
                        {
                            ReflectionsBakeParams.identifier.endpointInfluence.center = SteamAudio::ConvertVector(Task.BakedListener->GetOwner()->GetTransform().GetLocation());
                            ReflectionsBakeParams.identifier.endpointInfluence.radius = Task.BakedListener->InfluenceRadius;
                        }
                    }
                    else if (Task.Type == EBakeTaskType::REVERB)
                    {
                        ReflectionsBakeParams.identifier.variation = IPL_BAKEDDATAVARIATION_REVERB;
                    }
                }

                if (Task.Type == EBakeTaskType::PATHING)
                {
                    iplPathBakerBake(Context, &PathBakeParams, BakeProgressCallback, nullptr);
                }
                else
                {
                    iplReflectionsBakerBake(Context, &ReflectionsBakeParams, BakeProgressCallback, nullptr);
                }

                IPLBakedDataIdentifier& Identifier = (Task.Type == EBakeTaskType::PATHING) ? PathBakeParams.identifier : ReflectionsBakeParams.identifier;
                FString LayerName = Task.GetLayerName();
                int LayerSize = iplProbeBatchGetDataSize(ProbeBatch, &Identifier);

                SteamAudio::RunInGameThread<void>([&]()
                {
                    ProbeVolume->AddOrUpdateLayer(LayerName, Identifier, LayerSize);
                });

                NumBakesSucceededForProbeBatch++;
                GCurrentBakeTask++;
            }

            IPLSerializedObjectSettings SerializedObjectSettings{};

            IPLSerializedObject SerializedObject = nullptr;
            IPLerror Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
            if (Status != IPL_STATUS_SUCCESS)
            {
                UE_LOG(LogSteamAudioEditor, Warning, TEXT("Unable to create serialized object. [%d]"), Status);
                iplProbeBatchRelease(&ProbeBatch);
                GCurrentProbeVolume++;
                continue;
            }

            iplProbeBatchSave(ProbeBatch, SerializedObject);

            SteamAudio::RunInGameThread<void>([&]()
            {
                ProbeVolume->Asset = USteamAudioSerializedObject::SerializeObjectToPackage(SerializedObject, ProbeVolume->Asset.GetAssetPathString());
                ProbeVolume->UpdateTotalSize(iplSerializedObjectGetSize(SerializedObject));
                ProbeVolume->MarkPackageDirty();
            });

            iplSerializedObjectRelease(&SerializedObject);
            iplProbeBatchRelease(&ProbeBatch);
            NumBakesSucceeded += NumBakesSucceededForProbeBatch;
            GCurrentProbeVolume++;
        }

        iplStaticMeshRelease(&StaticMesh);
        Manager.ShutDownSteamAudio();
        Promise.SetValue(NumBakesSucceeded);
    });

    TFuture<int> Future = Promise.GetFuture();
    Future.Wait();

    int NumBakesSucceeded = Future.Get();

    if (NumBakesSucceeded == 0)
        return EBakeResult::FAILURE;
    else if (NumBakesSucceeded == GNumBakeTasks * GNumProbeVolumes)
        return EBakeResult::SUCCESS;
    else
        return EBakeResult::PARTIAL_SUCCESS;
}

void Bake(UWorld* World, ULevel* Level, const TArray<FBakeTask>& Tasks, FSteamAudioBakeComplete OnBakeComplete)
{
    check(World);
    check(Level);

    GIsBaking = true;

    FSteamAudioEditorModule::NotifyStartingWithCancel(NSLOCTEXT("SteamAudio", "Baking", "Baking..."), FSimpleDelegate::CreateStatic(CancelBake));

    ASteamAudioStaticMeshActor* StaticMeshActor = ASteamAudioStaticMeshActor::FindInLevel(World, Level);
    if (!StaticMeshActor || !StaticMeshActor->Asset.IsValid())
    {
        FSteamAudioEditorModule::NotifyFailed(NSLOCTEXT("SteamAudio", "BakeFailedNoScene", "Bake failed: no static geometry."));
        GIsBaking = false;
        return;
    }

    TArray<AActor*> ProbeVolumes;
    UGameplayStatics::GetAllActorsOfClass(World, ASteamAudioProbeVolume::StaticClass(), ProbeVolumes);

    if (ProbeVolumes.Num() <= 0)
    {
        FSteamAudioEditorModule::NotifyFailed(NSLOCTEXT("SteamAudio", "BakeFailedNoProbes", "Bake failed: no probe volumes."));
        GIsBaking = false;
        return;
    }

    GNumBakeTasks = Tasks.Num();
    GNumProbeVolumes = ProbeVolumes.Num();
    GCurrentBakeTask = 1;
    GCurrentProbeVolume = 1;

    Async(EAsyncExecution::Thread, [StaticMeshActor, ProbeVolumes, Tasks, OnBakeComplete]()
    {
        EBakeResult BakeResult = BakeInternal(StaticMeshActor, ProbeVolumes, Tasks);
        if (BakeResult == EBakeResult::SUCCESS)
        {
            FSteamAudioEditorModule::NotifySucceeded(NSLOCTEXT("SteamAudio", "BakeSucceeded", "Bake succeeded."));
        }
        else if (BakeResult == EBakeResult::PARTIAL_SUCCESS)
        {
            FSteamAudioEditorModule::NotifyFailed(NSLOCTEXT("SteamAudio", "BakePartiallySucceeded", "Bake completed, but with errors."));
        }
        else
        {
            FSteamAudioEditorModule::NotifyFailed(NSLOCTEXT("SteamAudio", "BakeFailed", "Bake failed."));
        }

        OnBakeComplete.ExecuteIfBound();
        GIsBaking = false;
    });
}

}
