//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioFMODStudio.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "FMODStudioModule.h"
#include "FMODStudio/Classes/FMODAudioComponent.h"
#include "SteamAudioSourceComponent.h"

#define LOCTEXT_NAMESPACE "FSteamAudioFMODStudioModule"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioFMODStudioModule
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioFMODStudioModule::StartupModule()
{
	FString BaseDir = IPluginManager::Get().FindPlugin("FMODStudio")->GetBaseDir();
#if PLATFORM_WINDOWS
#if PLATFORM_64BITS
	FString LibraryPath = BaseDir + TEXT("/Binaries/Win64/phonon_fmod.dll");
#else
	FString LibraryPath = BaseDir + TEXT("/Binaries/Win32/phonon_fmod.dll");
#endif
#elif PLATFORM_LINUX
	FString LibraryPath = BaseDir + TEXT("/Binaries/Linux/libphonon_fmod.so");
#elif PLATFORM_MAC
	FString LibraryPath = BaseDir + TEXT("/Binaries/Mac/libphonon_fmod.dylib");
#elif PLATFORM_ANDROID
#if PLATFORM_ANDROID_ARM
	FString LibraryPath = BaseDir + TEXT("/Binaries/Android/armeabi-v7a/libphonon_fmod.so");
#elif PLATFORM_ANDROID_ARM64
	FString LibraryPath = BaseDir + TEXT("/Binaries/Android/arm64-v8a/libphonon_fmod.so");
#elif PLATFORM_ANDROID_X86
	FString LibraryPath = BaseDir + TEXT("/Binaries/Android/x86/libphonon_fmod.so");
#elif PLATFORM_ANDROID_X64
	FString LibraryPath = BaseDir + TEXT("/Binaries/Android/x86_64/libphonon_fmod.so");
#endif
#endif

	Library = FPlatformProcess::GetDllHandle(*LibraryPath);
	check(Library);

	iplFMODGetVersion = (iplFMODGetVersion_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODGetVersion"));
	iplFMODInitialize = (iplFMODInitialize_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODInitialize"));
	iplFMODTerminate = (iplFMODTerminate_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODTerminate"));
	iplFMODSetHRTF = (iplFMODSetHRTF_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODSetHRTF"));
	iplFMODSetSimulationSettings = (iplFMODSetSimulationSettings_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODSetSimulationSettings"));
	iplFMODSetReverbSource = (iplFMODSetReverbSource_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODSetReverbSource"));
	iplFMODAddSource = (iplFMODAddSource_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODAddSource"));
	iplFMODRemoveSource = (iplFMODRemoveSource_t) FPlatformProcess::GetDllExport(Library, TEXT("iplFMODRemoveSource"));
}

void FSteamAudioFMODStudioModule::ShutdownModule()
{}

TSharedPtr<IAudioEngineState> FSteamAudioFMODStudioModule::CreateAudioEngineState()
{
	return MakeShared<FFMODStudioAudioEngineState>();
}


// ---------------------------------------------------------------------------------------------------------------------
// FFMODStudioAudioEngineState
// ---------------------------------------------------------------------------------------------------------------------

FFMODStudioAudioEngineState::FFMODStudioAudioEngineState()
	: StudioSystem(nullptr)
	, CoreSystem(nullptr)
{}

void FFMODStudioAudioEngineState::Initialize(IPLContext Context, IPLHRTF HRTF, const IPLSimulationSettings& SimulationSettings)
{
	FSteamAudioFMODStudioModule::Get().iplFMODInitialize(Context);
	FSteamAudioFMODStudioModule::Get().iplFMODSetHRTF(HRTF);
	FSteamAudioFMODStudioModule::Get().iplFMODSetSimulationSettings(SimulationSettings);
}

void FFMODStudioAudioEngineState::Destroy()
{
	if (FSteamAudioFMODStudioModule::Get().Library)
	{
		FSteamAudioFMODStudioModule::Get().iplFMODTerminate();
	}
}

void FFMODStudioAudioEngineState::SetHRTF(IPLHRTF HRTF)
{}

void FFMODStudioAudioEngineState::SetReverbSource(IPLSource Source)
{
	FSteamAudioFMODStudioModule::Get().iplFMODSetReverbSource(Source);
}

FVector FFMODStudioAudioEngineState::ConvertVectorFromFMODStudio(const FMOD_VECTOR& FMODStudioVector)
{
    FVector UnrealVector;
    UnrealVector.X = FMODStudioVector.z;
    UnrealVector.Y = FMODStudioVector.x;
    UnrealVector.Z = FMODStudioVector.y;

    UnrealVector.X /= 0.01f;
    UnrealVector.Y /= 0.01f;
    UnrealVector.Z /= 0.01f;
    
    return UnrealVector;
}

FTransform FFMODStudioAudioEngineState::GetListenerTransform()
{
	FTransform Transform;
	FVector Position, Right, Up, Ahead;
            
    if (StudioSystem)
    {
        FMOD_3D_ATTRIBUTES ListenerAttributes{};
        StudioSystem->getListenerAttributes(0, &ListenerAttributes);
        
        Position = ConvertVectorFromFMODStudio(ListenerAttributes.position);
        Ahead = ConvertVectorFromFMODStudio(ListenerAttributes.forward);
        Up = ConvertVectorFromFMODStudio(ListenerAttributes.up);
        Right = FVector::CrossProduct(Ahead, Up);
    }

    Transform = FTransform(Ahead, Right, Up, Position);

    return Transform;
}

IPLAudioSettings FFMODStudioAudioEngineState::GetAudioSettings()
{
	IPLAudioSettings AudioSettings{};

	FMOD::System* System = GetSystem();
	if (System)
	{
		int SamplingRate = 0;
		FMOD_SPEAKERMODE SpeakerMode = FMOD_SPEAKERMODE_DEFAULT;
		int NumSpeakers = 0;
		CoreSystem->getSoftwareFormat(&SamplingRate, &SpeakerMode, &NumSpeakers);

		unsigned int FrameSize = 0;
		int NumBuffers = 0;
		CoreSystem->getDSPBufferSize(&FrameSize, &NumBuffers);

		AudioSettings.samplingRate = SamplingRate;
		AudioSettings.frameSize = FrameSize;
	}

	return AudioSettings;
}

TSharedPtr<IAudioEngineSource> FFMODStudioAudioEngineState::CreateAudioEngineSource()
{
	return MakeShared<FFMODStudioAudioEngineSource>();
}

FMOD::System* FFMODStudioAudioEngineState::GetSystem()
{
	if (!CoreSystem)
	{
		if (IFMODStudioModule::IsAvailable())
		{
			StudioSystem = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
			if (StudioSystem)
			{
				StudioSystem->getCoreSystem(&CoreSystem);
			}
		}
	}

	return CoreSystem;
}


// ---------------------------------------------------------------------------------------------------------------------
// FFMODStudioAudioEngineSource
// ---------------------------------------------------------------------------------------------------------------------

FFMODStudioAudioEngineSource::FFMODStudioAudioEngineSource()
	: FMODAudioComponent(nullptr)
	, DSP(nullptr)
	, SourceComponent(nullptr)
	, Handle(-1)
{}

void FFMODStudioAudioEngineSource::Initialize(AActor* Actor)
{
	FMODAudioComponent = Actor->FindComponentByClass<UFMODAudioComponent>();

	SourceComponent = Actor->FindComponentByClass<USteamAudioSourceComponent>();
	if (SourceComponent)
	{
		Handle = FSteamAudioFMODStudioModule::Get().iplFMODAddSource(SourceComponent->GetSource());
	}
}

void FFMODStudioAudioEngineSource::Destroy()
{
	if (SourceComponent)
	{
		FSteamAudioFMODStudioModule::Get().iplFMODRemoveSource(Handle);
	}
}

void FFMODStudioAudioEngineSource::UpdateParameters(USteamAudioSourceComponent* SteamAudioSourceComponent)
{
	check(SteamAudioSourceComponent);

	const int kSimulationOutputsParamIndex = 33;

	FMOD::DSP* MyDSP = GetDSP();

	if (MyDSP)
	{
		MyDSP->setParameterInt(kSimulationOutputsParamIndex, Handle);
	}
}

FMOD::DSP* FFMODStudioAudioEngineSource::GetDSP()
{
	if (FMODAudioComponent && !DSP)
	{
		FMOD::Studio::EventInstance* EventInstance = FMODAudioComponent->StudioInstance;
		if (EventInstance)
		{
			FMOD::ChannelGroup* ChannelGroup = nullptr;
			FMOD_RESULT Result = EventInstance->getChannelGroup(&ChannelGroup);
			if (Result == FMOD_OK && ChannelGroup)
			{
				int NumDSPs = 0;
				Result = ChannelGroup->getNumDSPs(&NumDSPs);
				if (Result == FMOD_OK && NumDSPs > 0)
				{
					for (int i = 0; i < NumDSPs; ++i)
					{
						FMOD::DSP* CurDSP = nullptr;
						Result = ChannelGroup->getDSP(i, &CurDSP);
						if (Result == FMOD_OK && CurDSP)
						{
							char Name[64] = {0};
							unsigned int Version = 0;
							int Channels = 0;
							int Width = 0;
							int Height = 0;
							Result = CurDSP->getInfo(Name, &Version, &Channels, &Width, &Height);
							if (Result == FMOD_OK)
							{
								if (strncmp(Name, "Steam Audio Spatializer", 64) == 0)
								{
									DSP = CurDSP;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	return DSP;
}

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(SteamAudio::FSteamAudioFMODStudioModule, SteamAudioFMODStudio)
