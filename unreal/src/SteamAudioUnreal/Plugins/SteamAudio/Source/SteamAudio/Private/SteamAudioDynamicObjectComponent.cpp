//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioDynamicObjectComponent.h"
#include "GameFramework/Actor.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioDynamicObjectComponent
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioDynamicObjectComponent::USteamAudioDynamicObjectComponent()
    : Asset()
    , Scene(nullptr)
    , InstancedMesh(nullptr)
{
    // Enable ticking.
    bAutoActivate = true;
    PrimaryComponentTick.bCanEverTick = true;
}

FSoftObjectPath USteamAudioDynamicObjectComponent::GetAssetToLoad()
{
    FSoftObjectPath AssetToLoad = Asset;

    if (!AssetToLoad.IsAsset())
    {
        const USteamAudioDynamicObjectComponent* DefaultObject = GetDefault<USteamAudioDynamicObjectComponent>();
        if (DefaultObject)
        {
            AssetToLoad = DefaultObject->Asset;
        }
    }
    
    return AssetToLoad;
}

void USteamAudioDynamicObjectComponent::BeginPlay()
{
    Super::BeginPlay();

    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();

    FSoftObjectPath AssetToLoad = GetAssetToLoad();

    // If an asset isn't specified, then we haven't yet exported this dynamic object, so do nothing.
    if (!AssetToLoad.IsAsset())
        return;

    if (!Manager.InitializeSteamAudio(SteamAudio::EManagerInitReason::PLAYING))
        return;

    Scene = iplSceneRetain(Manager.GetScene());
    if (!Scene)
        return;

    InstancedMesh = Manager.LoadDynamicObject(this);
    if (!InstancedMesh)
    {
        iplSceneRelease(&Scene);
        return;
    }

    iplInstancedMeshAdd(InstancedMesh, Scene);
}

void USteamAudioDynamicObjectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();

    if (Scene && InstancedMesh)
    {
        iplInstancedMeshRemove(InstancedMesh, Scene);
        Manager.UnloadDynamicObject(this);
        iplInstancedMeshRelease(&InstancedMesh);
        iplSceneRelease(&Scene);
    }

    Super::EndPlay(EndPlayReason);
}

void USteamAudioDynamicObjectComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (Scene && InstancedMesh)
    {
        IPLMatrix4x4 Transform = SteamAudio::ConvertTransform(GetOwner()->GetRootComponent()->GetComponentTransform());
        iplInstancedMeshUpdateTransform(InstancedMesh, Scene, Transform);
    }
}
