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
#include "GameFramework/Actor.h"
#include "SteamAudioStaticMeshActor.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// ASteamAudioStaticMeshActor
// ---------------------------------------------------------------------------------------------------------------------

/**
 * An actor that references static geometry for a level.
 */
UCLASS(ClassGroup = (SteamAudio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API ASteamAudioStaticMeshActor : public AActor
{
    GENERATED_BODY()

public:
    /** Reference to the Steam Audio Serialized Object asset containing static geometry data. */
    UPROPERTY(VisibleAnywhere, Category = ExportSettings, meta = (AllowedClasses = "/Script/SteamAudio.SteamAudioSerializedObject"))
    FSoftObjectPath Asset;

    ASteamAudioStaticMeshActor();

    static ASteamAudioStaticMeshActor* FindInLevel(UWorld* World, ULevel* Level);

protected:
    /**
     * Inherited from UActorComponent
     */

    /** Called when the component has been initialized. */
    virtual void BeginPlay() override;

    /** Called when the component is going to be destroyed. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** Retained reference to the main scene used by the Steam Audio Manager for simulation. */
    IPLScene Scene;

    /** The Static Mesh object. */
    IPLStaticMesh StaticMesh;
};
