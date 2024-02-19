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

#include "SteamAudioModule.h"
#include "Components/ActorComponent.h"
#include "SteamAudioGeometryComponent.generated.h"

class AStaticMeshActor;

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioGeometryComponent
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Tags an actor (and optionally its children) as containing geometry that should be exported to Steam Audio. This
 * should be attached to Static Mesh actors or Landscape actors.
 */
UCLASS(ClassGroup = (SteamAudio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API USteamAudioGeometryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Reference to the material asset that should be applied to all triangles exported as part of this component. */
    UPROPERTY(EditAnywhere, Category = MaterialSettings, meta = (AllowedClasses = "/Script/SteamAudio.SteamAudioMaterial"))
    FSoftObjectPath Material;

    /** Whether or not to export all actors attached to this actor. */
    UPROPERTY(EditAnywhere, Category = ExportSettings)
    bool bExportAllChildren;

    /** The number of vertices exported to Steam Audio. */
    UPROPERTY(VisibleAnywhere, Category = GeometryStatistics, meta = (DisplayName = "Vertices"))
    int NumVertices;

    /** The number of triangles exported to Steam Audio. */
    UPROPERTY(VisibleAnywhere, Category = GeometryStatistics, meta = (DisplayName = "Triangles"))
    int NumTriangles;

    USteamAudioGeometryComponent();

    /**
     * Inherited from UActorComponent
     */

    /** Called when the component is first created. */
    virtual void OnComponentCreated() override;

#if WITH_EDITOR
    /** Called when some property of the component is changed. */
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    /** Recalculates the number of vertices and triangles that are exported as part of this component. */
    void UpdateStatistics();

    /** Calculates the number of vertices and triangles to export from a single Static Mesh Actor. */
    static void GetStatisticsForStaticMeshActor(AStaticMeshActor* StaticMeshActor, int& NumVertices, int& NumTriangles);

    /** Calculates the number of vertices and triangles to export from an Actor and all of its children. */
    static void GetStatisticsForActorAndChildren(AActor* Actor, int& NumVertices, int& NumTriangles);
};
