//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioStaticMeshActor.h"
#include "EngineUtils.h"
#include "SteamAudioManager.h"
#include "SteamAudioScene.h"

// ---------------------------------------------------------------------------------------------------------------------
// ASteamAudioStaticMeshActor
// ---------------------------------------------------------------------------------------------------------------------

ASteamAudioStaticMeshActor::ASteamAudioStaticMeshActor()
    : Asset()
    , Scene(nullptr)
    , StaticMesh(nullptr)
{}

void ASteamAudioStaticMeshActor::BeginPlay()
{
    Super::BeginPlay();

    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();

    // If an asset isn't specified, then we haven't yet exported this dynamic object, so do nothing.
    if (!Asset.IsAsset())
        return;

    if (!Manager.InitializeSteamAudio(SteamAudio::EManagerInitReason::PLAYING))
        return;

    Scene = iplSceneRetain(Manager.GetScene());
    if (!Scene)
        return;

    StaticMesh = SteamAudio::LoadStaticMeshFromAsset(Asset, Manager.GetContext(), Scene);
    if (!StaticMesh)
    {
        iplSceneRelease(&Scene);
        return;
    }
     
    iplStaticMeshAdd(StaticMesh, Scene);
}

void ASteamAudioStaticMeshActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SteamAudio::FSteamAudioManager& Manager = SteamAudio::FSteamAudioModule::GetManager();

    if (Scene && StaticMesh)
    {
        iplStaticMeshRemove(StaticMesh, Scene);
        iplStaticMeshRelease(&StaticMesh);
        iplSceneRelease(&Scene);
    }

    Super::EndPlay(EndPlayReason);
}

ASteamAudioStaticMeshActor* ASteamAudioStaticMeshActor::FindInLevel(UWorld* World, ULevel* Level)
{
    check(World);
    check(Level);

    for (TActorIterator<ASteamAudioStaticMeshActor> It(World); It; ++It)
    {
        if (It->GetLevel() == Level)
            return *It;
    }

    return nullptr;
}
