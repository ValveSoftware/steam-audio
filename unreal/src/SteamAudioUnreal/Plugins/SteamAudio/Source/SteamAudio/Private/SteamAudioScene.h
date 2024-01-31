//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "SteamAudioModule.h"

class USteamAudioDynamicObjectComponent;

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// Scene Export
// ---------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

/**
 * Returns true if the given (sub)level has any static geometry tagged for export.
 */
bool STEAMAUDIO_API DoesLevelHaveStaticGeometryForExport(UWorld* World, ULevel* Level);

/**
 * Exports static geometry for a single (sub)level. Can export either to a .uasset (for use at runtime) or to a .obj
 * (for debugging).
 */
bool STEAMAUDIO_API ExportStaticGeometryForLevel(UWorld* World, ULevel* Level, FString FileName, bool bExportOBJ = false);

/**
 * Exports geometry for a single dynamic object. Can export either to a .uasset (for use at runtime) or to a .obj (for
 * debugging). The dynamic object may be any actor in a level, or a blueprint.
 */
bool STEAMAUDIO_API ExportDynamicObject(USteamAudioDynamicObjectComponent* DynamicObject, FString FileName, bool bExportOBJ = false);

#endif


// ---------------------------------------------------------------------------------------------------------------------
// Scene Load/Unload
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Loads the geometry and material data in the given .uasset and creates a Static Mesh object from it.
 */
IPLStaticMesh STEAMAUDIO_API LoadStaticMeshFromAsset(FSoftObjectPath Asset, IPLContext Context, IPLScene Scene);


// ---------------------------------------------------------------------------------------------------------------------
// Baked Data Load/Unload
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Loads the probes and baked data in the given .uasset and creates a Probe Batch object from it.
 */
IPLProbeBatch STEAMAUDIO_API LoadProbeBatchFromAsset(FSoftObjectPath Asset, IPLContext Context);

}
