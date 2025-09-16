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

#include "SteamAudioDynamicObjectComponent.h"
#include "GameFramework/Actor.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioScene.h"

#ifdef WITH_EDITOR
#include "Subsystems/EditorAssetSubsystem.h"
#endif

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioDynamicObjectComponent
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioDynamicObjectComponent::USteamAudioDynamicObjectComponent()
    : Asset()
    , Scene(nullptr)
    , InstancedMesh(nullptr)
{
    PrimaryComponentTick.bCanEverTick = false;
}

FSoftObjectPath USteamAudioDynamicObjectComponent::GetAssetToLoad()
{
    return Asset;
}

void USteamAudioDynamicObjectComponent::BeginPlay()
{
    Super::BeginPlay();

    GetOwner()->GetRootComponent()->TransformUpdated.AddUObject(this, &USteamAudioDynamicObjectComponent::OnTransformUpdated);

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

void USteamAudioDynamicObjectComponent::OnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
    if (Scene && InstancedMesh)
    {
        FTransform RootComponentTransform = GetOwner()->GetRootComponent()->GetComponentTransform();
        RootComponentTransform.SetTranslation(GetOwner()->GetComponentsBoundingBox().GetCenter());
        IPLMatrix4x4 Transform = SteamAudio::ConvertTransform(RootComponentTransform);
        iplInstancedMeshUpdateTransform(InstancedMesh, Scene, Transform);
    }
}

#ifdef WITH_EDITOR
void USteamAudioDynamicObjectComponent::CleaupDynamicComponentAsset()
{
    if (Asset.IsValid() && bIsAssetActive)
    {
        auto EditorAssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
        if (EditorAssetSubsystem)
        {
            EditorAssetSubsystem->DeleteAsset(Asset.GetAssetPathString());
            bIsAssetActive = false;
        }
    }
}

void USteamAudioDynamicObjectComponent::DestroyComponent(bool bPromoteChildren)
{
    CleaupDynamicComponentAsset();
    Super::DestroyComponent(bPromoteChildren);
}

void USteamAudioDynamicObjectComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    CleaupDynamicComponentAsset();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}
#endif

void USteamAudioDynamicObjectComponent::ExportDynamicObjectRuntime()
{
    SteamAudio::ExportDynamicObjectRuntime(this, Scene, InstancedMesh);
}