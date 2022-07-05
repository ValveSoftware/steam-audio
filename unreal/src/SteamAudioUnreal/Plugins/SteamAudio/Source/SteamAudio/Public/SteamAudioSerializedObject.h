//
// Copyright (C) Valve Corporation. All rights reserved.
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
