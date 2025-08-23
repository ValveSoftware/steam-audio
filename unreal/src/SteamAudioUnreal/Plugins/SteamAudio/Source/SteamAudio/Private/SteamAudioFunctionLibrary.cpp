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

#include "SteamAudioFunctionLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "SteamAudioModule.h"
#include "SteamAudioManager.h"

using namespace SteamAudio;

bool USteamAudioFunctionLibrary::IsInitialized()
{
	return FSteamAudioModule::GetManager().IsInitialized();
}

void USteamAudioFunctionLibrary::SetSteamAudioEnabled(bool bNewIsSteamAudioEnabled)
{
	FSteamAudioModule::GetManager().SetSteamAudioEnabled(bNewIsSteamAudioEnabled);
}

bool USteamAudioFunctionLibrary::IsSteamAudioEnabled()
{
	return FSteamAudioModule::GetManager().IsSteamAudioEnabled();
}

void USteamAudioFunctionLibrary::UpdateStaticMesh()
{
	FSteamAudioModule::GetManager().UpdateStaticMesh();
}

void USteamAudioFunctionLibrary::UpdateStaticMeshMaterial(AStaticMeshActor* StaticMeshActor)
{
	FSteamAudioModule::GetManager().UpdateStaticMeshMaterial(StaticMeshActor);
}

void USteamAudioFunctionLibrary::ShutDownSteamAudio(bool bResetFlags)
{
	FSteamAudioModule::GetManager().ShutDownSteamAudio(bResetFlags);
}

void USteamAudioFunctionLibrary::UnloadDynamicObject(USteamAudioDynamicObjectComponent* DynamicObjectComponent)
{
	FSteamAudioModule::GetManager().UnloadDynamicObject(DynamicObjectComponent);
}

void USteamAudioFunctionLibrary::AddSource(USteamAudioSourceComponent* Source)
{
	FSteamAudioModule::GetManager().AddSource(Source);
}

void USteamAudioFunctionLibrary::RemoveSource(USteamAudioSourceComponent* Source)
{
	FSteamAudioModule::GetManager().RemoveSource(Source);
}

void USteamAudioFunctionLibrary::AddListener(USteamAudioListenerComponent* Listener)
{
	FSteamAudioModule::GetManager().AddListener(Listener);
}

void USteamAudioFunctionLibrary::RemoveListener(USteamAudioListenerComponent* Listener)
{
	FSteamAudioModule::GetManager().RemoveListener(Listener);
}