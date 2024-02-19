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

#include "SteamAudioGeometryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioGeometryComponent
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioGeometryComponent::USteamAudioGeometryComponent()
    : Material(nullptr)
    , bExportAllChildren(false)
    , NumVertices(0)
    , NumTriangles(0)
{
    // Disable ticking.
    PrimaryComponentTick.bCanEverTick = false;
}

void USteamAudioGeometryComponent::OnComponentCreated()
{
    Super::OnComponentCreated();

#if WITH_EDITOR
    UpdateStatistics();
#endif
}

#if WITH_EDITOR
void USteamAudioGeometryComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // If "Export All Children" was toggled, recalculate geometry statistics.
    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    if ((PropertyName == GET_MEMBER_NAME_CHECKED(USteamAudioGeometryComponent, bExportAllChildren)))
    {
        UpdateStatistics();
    }
}
#endif

void USteamAudioGeometryComponent::UpdateStatistics()
{
#if WITH_EDITOR
    if (bExportAllChildren)
    {
        GetStatisticsForActorAndChildren(GetOwner(), NumVertices, NumTriangles);
    }
    else
    {
        GetStatisticsForStaticMeshActor(Cast<AStaticMeshActor>(GetOwner()), NumVertices, NumTriangles);
    }
#endif
}

void USteamAudioGeometryComponent::GetStatisticsForStaticMeshActor(AStaticMeshActor* StaticMeshActor, int& NumVertices, int& NumTriangles)
{
    NumVertices = 0;
    NumTriangles = 0;

    if (!StaticMeshActor)
        return;

    UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
    if (!StaticMeshComponent)
        return;

    UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    if (!StaticMesh)
        return;

    FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
    if (!RenderData)
        return;

    const FStaticMeshLODResources& LODModel = RenderData->LODResources[0];

    NumVertices += LODModel.GetNumVertices();
    NumTriangles += LODModel.GetNumTriangles();
}

void USteamAudioGeometryComponent::GetStatisticsForActorAndChildren(AActor* Actor, int& NumVertices, int& NumTriangles)
{
    NumVertices = 0;
    NumTriangles = 0;

    if (!Actor)
        return;

    // First, get the statistics for this actor itself.
    GetStatisticsForStaticMeshActor(Cast<AStaticMeshActor>(Actor), NumVertices, NumTriangles);

    TArray<AActor*> AttachedActors;
    Actor->GetAttachedActors(AttachedActors);

    // Now recursively process its children.
    for (AActor* AttachedActor : AttachedActors)
    {
        int NumVerticesForActor = 0;
        int NumTrianglesForActor = 0;

        GetStatisticsForActorAndChildren(AttachedActor, NumVerticesForActor, NumTrianglesForActor);

        NumVertices += NumVerticesForActor;
        NumTriangles += NumTrianglesForActor;
    }
}
