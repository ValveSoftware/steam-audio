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
