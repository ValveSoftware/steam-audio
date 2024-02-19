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
