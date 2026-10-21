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

#include "SteamAudioDryInputCache.h"
#include "SteamAudioModule.h"
#include "HAL/UnrealMemory.h"
#include "Misc/ScopeLock.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioDryInputCache
// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<FSteamAudioDryInputCache, ESPMode::ThreadSafe> FSteamAudioDryInputCache::GetForDevice(const FAudioDevice* AudioDevice)
{
    // Weak, so a cache lives exactly as long as the plugin instances of its device. Devices come and go (e.g. one per
    // PIE session), and a recycled device address must not pick up a dead device's cache.
    static FCriticalSection DevicesLock;
    static TMap<const FAudioDevice*, TWeakPtr<FSteamAudioDryInputCache, ESPMode::ThreadSafe>> Devices;

    FScopeLock Lock(&DevicesLock);

    for (auto It = Devices.CreateIterator(); It; ++It)
    {
        if (!It->Value.IsValid())
        {
            It.RemoveCurrent();
        }
    }

    if (const TWeakPtr<FSteamAudioDryInputCache, ESPMode::ThreadSafe>* Existing = Devices.Find(AudioDevice))
    {
        return Existing->Pin().ToSharedRef();
    }

    TSharedRef<FSteamAudioDryInputCache, ESPMode::ThreadSafe> Cache = MakeShared<FSteamAudioDryInputCache, ESPMode::ThreadSafe>();
    Devices.Add(AudioDevice, Cache);
    UE_LOG(LogSteamAudio, Verbose, TEXT("Created dry input cache for audio device %p; %d live."), AudioDevice, Devices.Num());
    return Cache;
}

FSteamAudioDryInputCache::~FSteamAudioDryInputCache()
{
    UE_LOG(LogSteamAudio, Verbose, TEXT("Freed dry input cache (%d voices)."), Buffers.Num());
}

void FSteamAudioDryInputCache::Initialize(int32 NumSources, int32 InFrameSize)
{
    FScopeLock Lock(&InitLock);

    // Both the occlusion and spatialization plugins call this with the same values; only grow, never shrink, so a
    // second call can't free memory the other plugin is using.
    FrameSize = FMath::Max(FrameSize, InFrameSize);

    if (Buffers.Num() < NumSources)
    {
        Buffers.SetNum(NumSources);
        StoredFrames.SetNumZeroed(NumSources);
        bValid.SetNumZeroed(NumSources);
    }

    for (TArray<float>& Buffer : Buffers)
    {
        if (Buffer.Num() < FrameSize)
        {
            Buffer.SetNumZeroed(FrameSize);
        }
    }
}

void FSteamAudioDryInputCache::Store(uint32 SourceId, const float* InterleavedData, int32 NumChannels, int32 NumFrames)
{
    if (!Buffers.IsValidIndex(SourceId) || NumChannels <= 0 || NumFrames <= 0 || NumFrames > Buffers[SourceId].Num())
    {
        Invalidate(SourceId);
        return;
    }

    float* Dest = Buffers[SourceId].GetData();

    if (NumChannels == 1)
    {
        FMemory::Memcpy(Dest, InterleavedData, NumFrames * sizeof(float));
    }
    else
    {
        const float Scale = 1.0f / NumChannels;
        for (int32 i = 0; i < NumFrames; ++i)
        {
            float Sum = 0.0f;
            for (int32 j = 0; j < NumChannels; ++j)
            {
                Sum += InterleavedData[i * NumChannels + j];
            }
            Dest[i] = Sum * Scale;
        }
    }

    StoredFrames[SourceId] = NumFrames;
    bValid[SourceId] = true;
}

const float* FSteamAudioDryInputCache::Consume(uint32 SourceId, int32 NumFrames)
{
    if (!bValid.IsValidIndex(SourceId) || !bValid[SourceId])
    {
        return nullptr;
    }

    // Consumed either way, so a mismatched block can't be picked up by a later one.
    bValid[SourceId] = false;

    if (NumFrames > StoredFrames[SourceId])
    {
        return nullptr;
    }

    return Buffers[SourceId].GetData();
}

void FSteamAudioDryInputCache::Invalidate(uint32 SourceId)
{
    if (bValid.IsValidIndex(SourceId))
    {
        bValid[SourceId] = false;
    }
}

}
