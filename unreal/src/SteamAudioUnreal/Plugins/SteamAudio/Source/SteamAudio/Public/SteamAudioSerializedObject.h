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
#include "SteamAudioSerializedObject.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSerializedObject
// ---------------------------------------------------------------------------------------------------------------------

/**
 * An object containing data from an IPLSerializedObject that can be serialized to a .uasset file.
 */
UCLASS()
class STEAMAUDIO_API USteamAudioSerializedObject : public UObject
{
    GENERATED_BODY()

public:
    /** The data to serialize. */
    UPROPERTY()
    TArray<uint8> Data;

    /** Serializes the binary data in the provided IPLSerializedObject to a .uasset. The asset is specified using an
        Unreal asset path of the form /Path/To/PackageName.ObjectName. */
    static USteamAudioSerializedObject* SerializeObjectToPackage(IPLSerializedObject SerializedObject, const FString& AssetName);
};
