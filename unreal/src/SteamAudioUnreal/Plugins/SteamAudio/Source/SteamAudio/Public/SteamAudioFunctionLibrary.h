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

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamAudioFunctionLibrary.generated.h"

class USteamAudioDynamicObjectComponent;
class USteamAudioListenerComponent;
class USteamAudioSourceComponent;
class AStaticMeshActor;

UCLASS()
class STEAMAUDIO_API USteamAudioFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure)
	static bool IsInitialized();

	/** Sets the Steam Audio enabled mode. */
	UFUNCTION(BlueprintCallable)
	static void SetSteamAudioEnabled(bool bNewIsSteamAudioEnabled);

	/** Returns the Steam Audio enabled mode. */
	UFUNCTION(BlueprintPure)
	static bool IsSteamAudioEnabled();

	/** Updates the iplStaticMesh data.
	Note: If you want to create a new Static Geometry during gameplay using this function, you will need to enable the "AllowCPUAccess" flag in the Static Mesh settings of actor. */
	UFUNCTION(BlueprintCallable)
	static void UpdateStaticMesh();

	/** Updates the iplStaticMesh material data on specified StaticMeshActor. */
	UFUNCTION(BlueprintCallable)
	static void UpdateStaticMeshMaterial(AStaticMeshActor* StaticMeshActor);

	/** Shuts down the global Steam Audio state. */
	UFUNCTION(BlueprintCallable)
	static void ShutDownSteamAudio(bool bResetFlags = true);

	/** Releases the reference to the geometry and material data for the given Steam Audio Dynamic Mesh component.
		If the reference count reaches zero, the data is destroyed. */
	UFUNCTION(BlueprintCallable)
	static void UnloadDynamicObject(USteamAudioDynamicObjectComponent* DynamicObjectComponent);

	/** Registers a Steam Audio Source component for simulation. */
	UFUNCTION(BlueprintCallable)
	static void AddSource(USteamAudioSourceComponent* Source);

	/** Unregisters a Steam Audio Source component from simulation. */
	UFUNCTION(BlueprintCallable)
	static void RemoveSource(USteamAudioSourceComponent* Source);

	/** Registers a Steam Audio Listener component for simulation. */
	UFUNCTION(BlueprintCallable)
	static void AddListener(USteamAudioListenerComponent* Listener);

	/** Unregisters a Steam Audio Listener component from simulation. */
	UFUNCTION(BlueprintCallable)
	static void RemoveListener(USteamAudioListenerComponent* Listener);

	/** Disables HRTF globally. */
	UFUNCTION(BlueprintCallable, Category="SteamAudio")
	static void SetHRTFDisabled(bool bDisabled);
};
