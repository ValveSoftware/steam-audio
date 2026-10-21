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

class FAudioDevice;

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioDryInputCache
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Per-device store of each source's pre-occlusion (dry) mono input.
 *
 * Unreal's built-in audio engine runs the occlusion plugin before the spatialization plugin, on the same buffer. The
 * spatializer renders pathing, so without this cache pathing would be fed audio that has already been attenuated by
 * occlusion, making occluded sources (the case pathing exists for) nearly silent. The occlusion plugin stores its
 * input here, and the spatializer consumes it as the pathing input for the same audio block.
 *
 * A given source is always processed by one thread at a time, occlusion first, so per-source slots need no locking.
 */
class FSteamAudioDryInputCache
{
public:
    /** Returns the cache shared by all Steam Audio plugins on the given device, creating it if needed. */
    static TSharedRef<FSteamAudioDryInputCache, ESPMode::ThreadSafe> GetForDevice(const FAudioDevice* AudioDevice);

    ~FSteamAudioDryInputCache();

    /** Sizes the cache. Called from each plugin's Initialize. */
    void Initialize(int32 NumSources, int32 FrameSize);

    /** Stores the dry input for a source, downmixing to mono. */
    void Store(uint32 SourceId, const float* InterleavedData, int32 NumChannels, int32 NumFrames);

    /** Returns the dry input stored for this block and marks it used, or nullptr if nothing was stored. */
    const float* Consume(uint32 SourceId, int32 NumFrames);

    /** Discards any stored input for a source. */
    void Invalidate(uint32 SourceId);

private:
    FCriticalSection InitLock;
    int32 FrameSize = 0;
    TArray<TArray<float>> Buffers;
    TArray<int32> StoredFrames;
    TArray<bool> bValid;
};

}
