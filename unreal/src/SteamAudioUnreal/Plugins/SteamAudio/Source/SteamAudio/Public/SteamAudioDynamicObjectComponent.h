//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "SteamAudioModule.h"
#include "Components/ActorComponent.h"
#include "SteamAudioDynamicObjectComponent.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioDynamicObjectComponent
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Tags an actor and its children as belonging to a dynamic object. Dynamic objects are not included in a scene's
 * static geometry when exporting geometry to Steam Audio. A Steam Audio Dynamic Object component can be attached to
 * any actor in a level, or to a blueprint.
 */
UCLASS(ClassGroup = (SteamAudio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API USteamAudioDynamicObjectComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Reference to the .uasset containing this dynamic object's geometry data. */
    UPROPERTY(VisibleAnywhere, Category = ExportSettings, meta = (AllowedClasses = "/Script/SteamAudio.SteamAudioSerializedObject"))
    FSoftObjectPath Asset;

    USteamAudioDynamicObjectComponent();

    /**
     * Inherited from UActorComponent
     */

    /** Called once every frame. */
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    FSoftObjectPath GetAssetToLoad();

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

    /** The Instanced Mesh object. */
    IPLInstancedMesh InstancedMesh;
};
