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

#include "SteamAudioWwise.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "AkAudioDevice.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "SteamAudioSourceComponent.h"

#define LOCTEXT_NAMESPACE "FSteamAudioWwiseModule"

#if PLATFORM_IOS
extern "C" {
void AKSOUNDENGINE_CALL iplWwiseGetVersion(unsigned int* Major, unsigned int* Minor, unsigned int* Patch);
void AKSOUNDENGINE_CALL iplWwiseInitialize(IPLContext Context, IPLWwiseSettings* Settings);
void AKSOUNDENGINE_CALL iplWwiseTerminate();
void AKSOUNDENGINE_CALL iplWwiseSetHRTF(IPLHRTF HRTF);
void AKSOUNDENGINE_CALL iplWwiseSetSimulationSettings(IPLSimulationSettings SimulationSettings);
void AKSOUNDENGINE_CALL iplWwiseSetReverbSource(IPLSource ReverbSource);
IPLint32 AKSOUNDENGINE_CALL iplWwiseAddSource(AkGameObjectID GameObjectID, IPLSource Source);
void AKSOUNDENGINE_CALL iplWwiseRemoveSource(AkGameObjectID GameObjectID);
}
#endif

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioWwiseModule
// ---------------------------------------------------------------------------------------------------------------------

FString FSteamAudioWwiseModule::GetDynamicLibraryPath(FString LibName)
{
	FString LibraryPath;

#if PLATFORM_64BITS
#define AK_WINDOWS_ARCHITECTURE "x64_"
#else
#define AK_WINDOWS_ARCHITECTURE "Win32_"
#endif

#if PLATFORM_WINDOWS
#ifdef AK_WINDOWS_VS_VERSION
	constexpr auto PlatformArchitecture = AK_WINDOWS_ARCHITECTURE AK_WINDOWS_VS_VERSION;
#else
	constexpr auto PlatformArchitecture = AK_WINDOWS_ARCHITECTURE "vc160";
#endif
	LibraryPath = FAkPlatform::GetDSPPluginsDirectory(PlatformArchitecture);
	LibraryPath += (LibName + ".dll");
#elif PLATFORM_LINUX
	constexpr auto PlatformArchitecture = "Linux_x64";
	LibraryPath = FAkPlatform::GetDSPPluginsDirectory(PlatformArchitecture);
	LibraryPath += ("lib" + LibName + ".so");
#elif PLATFORM_MAC
	constexpr auto PlatformArchitecture = "Mac_Xcode1400";
	LibraryPath = FAkPlatform::GetDSPPluginsDirectory(PlatformArchitecture);
	LibraryPath += ("lib" + LibName + ".dylib");
#elif PLATFORM_ANDROID
	LibraryPath = TEXT("lib" + LibName + ".so");
#endif
	
#undef AK_WINDOWS_ARCHITECTURE
	return LibraryPath;
}

void FSteamAudioWwiseModule::StartupModule()
{
	FString LibraryPath = GetDynamicLibraryPath("SteamAudioWwise");

#if PLATFORM_IOS
	this->iplWwiseGetVersion = (iplWwiseGetVersion_t) ::iplWwiseGetVersion;
	this->iplWwiseInitialize = (iplWwiseInitialize_t) ::iplWwiseInitialize;
	this->iplWwiseTerminate = (iplWwiseTerminate_t) ::iplWwiseTerminate;
	this->iplWwiseSetHRTF = (iplWwiseSetHRTF_t) ::iplWwiseSetHRTF;
	this->iplWwiseSetSimulationSettings = (iplWwiseSetSimulationSettings_t) ::iplWwiseSetSimulationSettings;
	this->iplWwiseSetReverbSource = (iplWwiseSetReverbSource_t) ::iplWwiseSetReverbSource;
	this->iplWwiseAddSource = (iplWwiseAddSource_t) ::iplWwiseAddSource;
	this->iplWwiseRemoveSource = (iplWwiseRemoveSource_t) ::iplWwiseRemoveSource;
#else
	Library = FPlatformProcess::GetDllHandle(*LibraryPath);
	check(Library);

	iplWwiseGetVersion = (iplWwiseGetVersion_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseGetVersion"));
	iplWwiseInitialize = (iplWwiseInitialize_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseInitialize"));
	iplWwiseTerminate = (iplWwiseTerminate_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseTerminate"));
	iplWwiseSetHRTF = (iplWwiseSetHRTF_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseSetHRTF"));
	iplWwiseSetSimulationSettings = (iplWwiseSetSimulationSettings_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseSetSimulationSettings"));
	iplWwiseSetReverbSource = (iplWwiseSetReverbSource_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseSetReverbSource"));
	iplWwiseAddSource = (iplWwiseAddSource_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseAddSource"));
	iplWwiseRemoveSource = (iplWwiseRemoveSource_t) FPlatformProcess::GetDllExport(Library, TEXT("iplWwiseRemoveSource"));
#endif
}

void FSteamAudioWwiseModule::ShutdownModule()
{}

TSharedPtr<IAudioEngineState> FSteamAudioWwiseModule::CreateAudioEngineState()
{
	return MakeShared<FWwiseAudioEngineState>();
}


// ---------------------------------------------------------------------------------------------------------------------
// FWwiseStudioAudioEngineState
// ---------------------------------------------------------------------------------------------------------------------

FWwiseAudioEngineState::FWwiseAudioEngineState()
{}

void FWwiseAudioEngineState::Initialize(IPLContext Context, IPLHRTF HRTF, const IPLSimulationSettings& SimulationSettings)
{
	IPLWwiseSettings WwiseSettings{};
	WwiseSettings.MetersPerUnit = 0.01f;

	FSteamAudioWwiseModule::Get().iplWwiseInitialize(Context, &WwiseSettings);
	FSteamAudioWwiseModule::Get().iplWwiseSetHRTF(HRTF);
	FSteamAudioWwiseModule::Get().iplWwiseSetSimulationSettings(SimulationSettings);
}

void FWwiseAudioEngineState::Destroy()
{
	if (FSteamAudioWwiseModule::Get().Library)
	{
		FSteamAudioWwiseModule::Get().iplWwiseTerminate();
	}
}

void FWwiseAudioEngineState::SetHRTF(IPLHRTF HRTF)
{}

void FWwiseAudioEngineState::SetReverbSource(IPLSource Source)
{
	FSteamAudioWwiseModule::Get().iplWwiseSetReverbSource(Source);
}

FVector FWwiseAudioEngineState::ConvertVectorFromWwise(const AkVector& WwiseVector)
{
	// Wwise distance units will match the distance units that you are using in game, for example, centimeters, meters, and so on.
	// https://www.audiokinetic.com/library/2023.1.6_8555/?source=Help&id=positioning_attenuation_editor

	FVector UnrealVector;
	FAkAudioDevice::AKVectorToFVector(WwiseVector, UnrealVector);

	return UnrealVector;
}

FTransform FWwiseAudioEngineState::GetListenerTransform()
{
	// This returns the transform of the first default listener. There can be multiple default listeners,
	// and individual sources can have their own listeners instead of the default listener, but these are
	// all ignored here.

	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (!AudioDevice)
		return FTransform{};

	const UAkComponentSet& DefaultListeners = AudioDevice->GetDefaultListeners();
	if (DefaultListeners.IsEmpty())
		return FTransform{};

	return DefaultListeners.Array()[0]->GetComponentTransform();
}

IPLAudioSettings FWwiseAudioEngineState::GetAudioSettings()
{
	IPLAudioSettings AudioSettings{};

	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine))
		return AudioSettings;

	AkAudioSettings WwiseAudioSettings;
	SoundEngine->GetAudioSettings(WwiseAudioSettings);

	AudioSettings.frameSize = WwiseAudioSettings.uNumSamplesPerFrame;
	AudioSettings.samplingRate = static_cast<IPLint32>(SoundEngine->GetSampleRate());
	
	return AudioSettings;
}

TSharedPtr<IAudioEngineSource> FWwiseAudioEngineState::CreateAudioEngineSource()
{
	return MakeShared<FWwiseAudioEngineSource>();
}


// ---------------------------------------------------------------------------------------------------------------------
// FWwiseAudioEngineSource
// ---------------------------------------------------------------------------------------------------------------------

FWwiseAudioEngineSource::FWwiseAudioEngineSource()
	: AkComponent(nullptr)
	, GameObjectID(AK_INVALID_GAME_OBJECT)
	, SourceComponent(nullptr)
{}

void FWwiseAudioEngineSource::Initialize(AActor* Actor)
{
	AkComponent = Actor->FindComponentByClass<UAkComponent>();
	if (!AkComponent)
		return;

	GameObjectID = AkComponent->GetAkGameObjectID();
	if (GameObjectID == AK_INVALID_GAME_OBJECT)
		return;

	SourceComponent = Actor->FindComponentByClass<USteamAudioSourceComponent>();
	if (!SourceComponent)
		return;

	IPLSource Source = SourceComponent->GetSource();
	if (!Source)
		return;

	FSteamAudioWwiseModule::Get().iplWwiseAddSource(GameObjectID, Source);
}

void FWwiseAudioEngineSource::Destroy()
{
	if (GameObjectID == AK_INVALID_GAME_OBJECT)
		return;

	FSteamAudioWwiseModule::Get().iplWwiseRemoveSource(GameObjectID);
}

void FWwiseAudioEngineSource::UpdateParameters(USteamAudioSourceComponent* SteamAudioSourceComponent)
{
	// Nothing to do here, simulation outputs are linked via AkGameObjectID.
}

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(SteamAudio::FSteamAudioWwiseModule, SteamAudioWwise)
