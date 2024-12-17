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

#include "SteamAudioManager.h"
#include "AudioDevice.h"
#include "Async/Async.h"
#include "HAL/UnrealMemory.h"
#include "SteamAudioAudioEngineInterface.h"
#include "SteamAudioCommon.h"
#include "SteamAudioDynamicObjectComponent.h"
#include "SteamAudioListenerComponent.h"
#include "SteamAudioScene.h"
#include "SteamAudioSettings.h"
#include "SteamAudioSourceComponent.h"
#include "SOFAFile.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioPluginListener
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioPluginListener::OnListenerUpdated(FAudioDevice* AudioDevice, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
{
    ListenerCoordinates.origin = SteamAudio::ConvertVector(ListenerTransform.GetLocation());
    ListenerCoordinates.ahead = SteamAudio::ConvertVector(ListenerTransform.GetUnitAxis(EAxis::X), false);
    ListenerCoordinates.up = SteamAudio::ConvertVector(ListenerTransform.GetUnitAxis(EAxis::Z), false);
    ListenerCoordinates.right = SteamAudio::ConvertVector(ListenerTransform.GetUnitAxis(EAxis::Y), false);
}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioManager
// ---------------------------------------------------------------------------------------------------------------------

FSteamAudioManager::FSteamAudioManager()
    : Context(nullptr)
    , HRTF(nullptr)
    , EmbreeDevice(nullptr)
    , OpenCLDevice(nullptr)
    , RadeonRaysDevice(nullptr)
    , TrueAudioNextDevice(nullptr)
    , Scene(nullptr)
    , Simulator(nullptr)
    , bInitializationAttempted(false)
    , bInitializationSucceded(false)
    , SteamAudioSettings()
    , bSettingsLoaded(false)
    , SimulationUpdateTimeElapsed(0.0f)
    , ThreadPool(nullptr)
    , ThreadPoolIdle(true)
{
    IPLContextSettings ContextSettings{};
    ContextSettings.version = STEAMAUDIO_VERSION;
    ContextSettings.logCallback = LogCallback;
    ContextSettings.allocateCallback = AllocateCallback;
    ContextSettings.freeCallback = FreeCallback;
    ContextSettings.simdLevel = IPL_SIMDLEVEL_AVX2;

    const USteamAudioSettings* Settings = GetDefault<USteamAudioSettings>();
    if (Settings)
    {
        if (Settings->EnableValidation)
        {
            ContextSettings.flags = static_cast<IPLContextFlags>(ContextSettings.flags | IPL_CONTEXTFLAGS_VALIDATION);
        }
    }

    IPLerror Status = iplContextCreate(&ContextSettings, &Context);
    if (Status != IPL_STATUS_SUCCESS)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create context. [%d]"), Status);
    }

    AudioPluginListener = TAudioPluginListenerPtr(new FSteamAudioPluginListener());
}

FSteamAudioManager::~FSteamAudioManager()
{
    ShutDownSteamAudio();
}

IPLCoordinateSpace3 FSteamAudioManager::GetListenerCoordinates()
{
    IPLCoordinateSpace3 ListenerCoordinates{};

    if (GetDefault<USteamAudioSettings>()->AudioEngine == EAudioEngineType::UNREAL)
    {
        // We're running Unreal's built-in audio engine, so use the data provided via the IAudioPluginListener API.
        if (AudioPluginListener)
        {
            ListenerCoordinates = StaticCastSharedPtr<FSteamAudioPluginListener>(AudioPluginListener)->GetListenerCoordinates();
        }
    }
    else
    {
        // We're using third-party audio middleware, so use the Steam Audio IAudioEngineState API to get the
        // listener transform.
        IAudioEngineState* AudioEngineState = FSteamAudioModule::GetAudioEngineState();
        if (AudioEngineState)
        {
            FTransform ListenerTransform = AudioEngineState->GetListenerTransform();

            ListenerCoordinates.origin = SteamAudio::ConvertVector(ListenerTransform.GetLocation());
            ListenerCoordinates.ahead = SteamAudio::ConvertVector(ListenerTransform.GetUnitAxis(EAxis::X), false);
            ListenerCoordinates.up = SteamAudio::ConvertVector(ListenerTransform.GetUnitAxis(EAxis::Z), false);
            ListenerCoordinates.right = SteamAudio::ConvertVector(ListenerTransform.GetUnitAxis(EAxis::Y), false);
        }
    }

    return ListenerCoordinates;
}

bool FSteamAudioManager::InitHRTF(IPLAudioSettings& AudioSettings)
{
    // If we're using Unreal's built-in audio engine, we may have already initialized the HRTF when the
    // spatialization plugin was initialized. In that case, do nothing.
    if (HRTF)
        return true;

    IPLHRTFSettings HRTFSettings{};
    HRTFSettings.type = IPL_HRTFTYPE_DEFAULT;
    HRTFSettings.volume = 1.0f;
    HRTFSettings.normType = IPL_HRTFNORMTYPE_NONE;

    const USteamAudioSettings* Settings = GetDefault<USteamAudioSettings>();
    if (Settings)
    {
        HRTFSettings.volume = SteamAudio::ConvertDbToLinear(Settings->HRTFVolume);
        HRTFSettings.normType = static_cast<IPLHRTFNormType>(Settings->HRTFNormalizationType);

        if (Settings->SOFAFile.IsValid())
        {
            USOFAFile* SOFAFile = Cast<USOFAFile>(Settings->SOFAFile.TryLoad());
            if (SOFAFile)
            {
                HRTFSettings.type = IPL_HRTFTYPE_SOFA;
                HRTFSettings.sofaData = SOFAFile->Data.GetData();
                HRTFSettings.sofaDataSize = SOFAFile->Data.Num();
                HRTFSettings.volume = SteamAudio::ConvertDbToLinear(SOFAFile->Volume);
                HRTFSettings.normType = static_cast<IPLHRTFNormType>(SOFAFile->NormalizationType);
            }
        }
    }

    IPLerror Status = iplHRTFCreate(Context, &AudioSettings, &HRTFSettings, &HRTF);
    if (Status == IPL_STATUS_SUCCESS)
    {
        return true;
    }
    else
    {
        if (HRTFSettings.type == IPL_HRTFTYPE_SOFA)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create HRTF from SOFA file %s, reverting to default HRTF. [%d]"), *Settings->SOFAFile.GetAssetPathString(), Status);

            HRTFSettings.type = IPL_HRTFTYPE_DEFAULT;

            Status = iplHRTFCreate(Context, &AudioSettings, &HRTFSettings, &HRTF);
            if (Status == IPL_STATUS_SUCCESS)
                return true;
        }

        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create HRTF. [%d]"), Status);
        return false;
    }
}

bool FSteamAudioManager::InitializeSteamAudio(EManagerInitReason Reason)
{
    // We already tried initializing before, so just return a flag indicating whether or not we succeeded when we last
    // tried.
    if (bInitializationAttempted)
        return bInitializationSucceded;

    bInitializationAttempted = true;

    const USteamAudioSettings* Settings = GetDefault<USteamAudioSettings>();
    if (!Settings)
    {
        bInitializationSucceded = false;
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to load Steam Audio Settings."));
        return false;
    }

    SteamAudioSettings = Settings->GetSettings();
    bSettingsLoaded = true;

    IPLSceneType ConfiguredSceneType = SteamAudioSettings.SceneType;
    IPLReflectionEffectType ConfiguredReflectionEffectType = SteamAudioSettings.ReflectionEffectType;

    ActualSceneType = ConfiguredSceneType;
    ActualReflectionEffectType = ConfiguredReflectionEffectType;

    if (Reason == EManagerInitReason::EXPORTING_SCENE || Reason == EManagerInitReason::GENERATING_PROBES)
    {
        ActualSceneType = IPL_SCENETYPE_DEFAULT;
    }

    bool bShouldInitEmbree = (Reason == EManagerInitReason::BAKING || Reason == EManagerInitReason::PLAYING) && (ConfiguredSceneType == IPL_SCENETYPE_EMBREE);
    bool bShouldInitRadeonRays = (Reason == EManagerInitReason::BAKING || Reason == EManagerInitReason::PLAYING) && (ConfiguredSceneType == IPL_SCENETYPE_RADEONRAYS);
    bool bShouldInitTrueAudioNext = (Reason == EManagerInitReason::PLAYING) && (ConfiguredReflectionEffectType == IPL_REFLECTIONEFFECTTYPE_TAN);
    bool bShouldInitOpenCL = (bShouldInitRadeonRays || bShouldInitTrueAudioNext);

    if (bShouldInitEmbree)
    {
        check(!EmbreeDevice);

        IPLerror Status = iplEmbreeDeviceCreate(Context, nullptr, &EmbreeDevice);
        if (Status != IPL_STATUS_SUCCESS)
        {
            ActualSceneType = IPL_SCENETYPE_DEFAULT;
            UE_LOG(LogSteamAudio, Warning, TEXT("Unable to initialize Embree device. [%d] Falling back to default."), Status);
        }
    }

    if (bShouldInitOpenCL)
    {
        check(!OpenCLDevice);

        IPLOpenCLDeviceSettings OpenCLDeviceSettings{};
        OpenCLDeviceSettings.type = SteamAudioSettings.OpenCLDeviceType;
        OpenCLDeviceSettings.numCUsToReserve = SteamAudioSettings.MaxReservedComputeUnits;
        OpenCLDeviceSettings.fractionCUsForIRUpdate = SteamAudioSettings.FractionComputeUnitsForIRUpdate;
        OpenCLDeviceSettings.requiresTAN = (ConfiguredReflectionEffectType == IPL_REFLECTIONEFFECTTYPE_TAN) ? IPL_TRUE : IPL_FALSE;

        IPLOpenCLDeviceList OpenCLDeviceList = nullptr;
        IPLerror Status = iplOpenCLDeviceListCreate(Context, &OpenCLDeviceSettings, &OpenCLDeviceList);
        if (Status != IPL_STATUS_SUCCESS)
        {
            int NumDevices = iplOpenCLDeviceListGetNumDevices(OpenCLDeviceList);

            if (NumDevices <= 0 && !OpenCLDeviceSettings.requiresTAN && OpenCLDeviceSettings.numCUsToReserve > 0)
            {
                // We didn't find any devices, but we had CU reservation specified even though we're not using TAN. So try
                // initializing without CU reservation.
                UE_LOG(LogSteamAudio, Warning, TEXT("No OpenCL devices found that match the provided parameters, attempting to initialize without CU reservation."));

                iplOpenCLDeviceListRelease(&OpenCLDeviceList);

                OpenCLDeviceSettings.numCUsToReserve = 0;
                OpenCLDeviceSettings.fractionCUsForIRUpdate = 0.0f;

                Status = iplOpenCLDeviceListCreate(Context, &OpenCLDeviceSettings, &OpenCLDeviceList);
                if (Status == IPL_STATUS_SUCCESS)
                {
                    NumDevices = iplOpenCLDeviceListGetNumDevices(OpenCLDeviceList);
                }
            }

            if (NumDevices > 0)
            {
                Status = iplOpenCLDeviceCreate(Context, OpenCLDeviceList, 0, &OpenCLDevice);
                if (Status != IPL_STATUS_SUCCESS)
                {
                    UE_LOG(LogSteamAudio, Warning, TEXT("Unable to create OpenCL device. [%d]"), Status);
                }
            }
            else
            {
                UE_LOG(LogSteamAudio, Warning, TEXT("No OpenCL devices found."));
            }

            iplOpenCLDeviceListRelease(&OpenCLDeviceList);
        }
        else
        {
            UE_LOG(LogSteamAudio, Warning, TEXT("Unable to create OpenCL device list. [%d]"), Status);
        }
    }

    if (bShouldInitRadeonRays)
    {
        check(!RadeonRaysDevice);

        IPLerror Status = iplRadeonRaysDeviceCreate(OpenCLDevice, nullptr, &RadeonRaysDevice);
        if (Status != IPL_STATUS_SUCCESS)
        {
            ActualSceneType = IPL_SCENETYPE_DEFAULT;
            UE_LOG(LogSteamAudio, Warning, TEXT("Unable to initialize Radeon Rays device. [%d] Falling back to default."), Status);
        }
    }

    check(!Scene);

    IPLSceneSettings SceneSettings{};
    SceneSettings.type = static_cast<IPLSceneType>(ActualSceneType);
    SceneSettings.embreeDevice = EmbreeDevice;
    SceneSettings.radeonRaysDevice = RadeonRaysDevice;

    IPLerror Status = iplSceneCreate(Context, &SceneSettings, &Scene);
    if (Status != IPL_STATUS_SUCCESS)
    {
        ShutDownSteamAudio(false);
        bInitializationSucceded = false;
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create scene. [%d]"), Status);
        return false;
    }

    if (Reason == EManagerInitReason::BAKING || Reason == EManagerInitReason::PLAYING)
    {
        // Set up communication with the audio engine.
        IAudioEngineStateFactory* AudioEngineStateFactory = nullptr;

        if (SteamAudioSettings.AudioEngine == EAudioEngineType::FMODSTUDIO)
        {
            // We're using FMOD Studio, so try to load the corresponding support plugin. If this is not enabled in
            // project settings, this step will fail.
            AudioEngineStateFactory = FModuleManager::LoadModulePtr<IAudioEngineStateFactory>(TEXT("SteamAudioFMODStudio"));
        }

        if (SteamAudioSettings.AudioEngine == EAudioEngineType::WWISE)
        {
            // We're using Wwise, so try to load the corresponding support plugin. If this is not enabled in
            // project settings, this step will fail.
            AudioEngineStateFactory = FModuleManager::LoadModulePtr<IAudioEngineStateFactory>(TEXT("SteamAudioWwise"));
        }

        if (!AudioEngineStateFactory)
        {
            // We are either configured to use Unreal's built-in audio engine, or loading the support plugin for
            // third-party middleware failed, so fall back to using the built-in audio engine.
            AudioEngineStateFactory = &FSteamAudioModule::Get();
        }

        check(AudioEngineStateFactory);

        FSteamAudioModule::SetAudioEngineState(AudioEngineStateFactory->CreateAudioEngineState());
    }

    if (Reason == EManagerInitReason::PLAYING)
    {
        check(!Simulator);

        IPLSimulationSettings SimulationSettings = GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));
        SimulationSettings.openCLDevice = OpenCLDevice;
        SimulationSettings.radeonRaysDevice = RadeonRaysDevice;
        SimulationSettings.tanDevice = TrueAudioNextDevice;

        Status = iplSimulatorCreate(Context, &SimulationSettings, &Simulator);
        if (Status != IPL_STATUS_SUCCESS)
        {
            ShutDownSteamAudio(false);
            bInitializationSucceded = false;
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create simulator. [%d]"), Status);
            return false;
        }

        if (!ThreadPool)
        {
            ThreadPool = FQueuedThreadPool::Allocate();
            if (ThreadPool)
            {
                ThreadPool->Create(1);
            }
        }

        ThreadPoolIdle = true;

        IAudioEngineState* AudioEngineState = FSteamAudioModule::GetAudioEngineState();

        IPLAudioSettings AudioSettings{};
        if (AudioEngineState)
        {
            AudioSettings = AudioEngineState->GetAudioSettings();
        }

        if (!InitHRTF(AudioSettings))
        {
            ShutDownSteamAudio(false);
            bInitializationSucceded = false;
            return false;
        }

        if (bShouldInitTrueAudioNext)
        {
            check(!TrueAudioNextDevice);

            IPLTrueAudioNextDeviceSettings TrueAudioNextSettings{};
            TrueAudioNextSettings.frameSize = AudioSettings.frameSize;
            TrueAudioNextSettings.irSize = CalcIRSizeForDuration(SteamAudioSettings.TANDuration, AudioSettings.samplingRate);
            TrueAudioNextSettings.order = SteamAudioSettings.TANAmbisonicOrder;
            TrueAudioNextSettings.maxSources = SteamAudioSettings.TANMaxSources;

            Status = iplTrueAudioNextDeviceCreate(OpenCLDevice, &TrueAudioNextSettings, &TrueAudioNextDevice);
            if (Status != IPL_STATUS_SUCCESS)
            {
                ActualReflectionEffectType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
                UE_LOG(LogSteamAudio, Warning, TEXT("Unable to initialize TrueAudio Next device. [%d] Falling back to convolution."), Status);
            }
        }

        if (AudioEngineState)
        {
            AudioEngineState->Initialize(Context, HRTF, SimulationSettings);
        }
    }

    bInitializationSucceded = true;
    return true;
}

void FSteamAudioManager::ShutDownSteamAudio(bool bResetFlags /* = true */)
{
    if (!bInitializationAttempted)
        return;

    IAudioEngineState* AudioEngineState = FSteamAudioModule::GetAudioEngineState();
    if (AudioEngineState)
    {
        AudioEngineState->Destroy();
    }

    FSteamAudioModule::SetAudioEngineState(nullptr);

    iplHRTFRelease(&HRTF);

    if (ThreadPool)
    {
        ThreadPool->Destroy();
        ThreadPool = nullptr;
        ThreadPoolIdle = true;
        SimulationUpdateTimeElapsed = 0.0f;
    }

    iplSimulatorRelease(&Simulator);
    iplSceneRelease(&Scene);
    iplTrueAudioNextDeviceRelease(&TrueAudioNextDevice);
    iplRadeonRaysDeviceRelease(&RadeonRaysDevice);
    iplOpenCLDeviceRelease(&OpenCLDevice);
    iplEmbreeDeviceRelease(&EmbreeDevice);

    if (bResetFlags)
    {
        bInitializationAttempted = false;
        bInitializationSucceded = false;
        bSettingsLoaded = false;
    }
}

void FSteamAudioManager::RegisterAudioPluginListener(FAudioDevice* OwningDevice)
{
    check(OwningDevice);
    OwningDevice->RegisterPluginListener(AudioPluginListener);
}

IPLSimulationSettings FSteamAudioManager::GetRealTimeSettings(IPLSimulationFlags Flags)
{
    check(bSettingsLoaded);

    IPLAudioSettings AudioSettings{};
    IAudioEngineState* AudioEngineState = FSteamAudioModule::GetAudioEngineState();
    if (AudioEngineState)
    {
        AudioSettings = AudioEngineState->GetAudioSettings();
    }

    IPLSimulationSettings SimulationSettings{};
    SimulationSettings.flags = Flags;
    SimulationSettings.sceneType = ActualSceneType;
    SimulationSettings.reflectionType = ActualReflectionEffectType;
    SimulationSettings.maxNumOcclusionSamples = SteamAudioSettings.MaxOcclusionSamples;
    SimulationSettings.maxNumRays = SteamAudioSettings.RealTimeRays;
    SimulationSettings.numDiffuseSamples = 32;
    SimulationSettings.maxDuration = (SteamAudioSettings.ReflectionEffectType == IPL_REFLECTIONEFFECTTYPE_TAN) ? SteamAudioSettings.TANDuration : SteamAudioSettings.RealTimeDuration;
    SimulationSettings.maxOrder = (SteamAudioSettings.ReflectionEffectType == IPL_REFLECTIONEFFECTTYPE_TAN) ? SteamAudioSettings.TANAmbisonicOrder : SteamAudioSettings.RealTimeAmbisonicOrder;
    SimulationSettings.maxNumSources = (SteamAudioSettings.ReflectionEffectType == IPL_REFLECTIONEFFECTTYPE_TAN) ? SteamAudioSettings.TANMaxSources : SteamAudioSettings.RealTimeMaxSources;
    SimulationSettings.numThreads = GetNumThreadsForCPUCoresPercentage(SteamAudioSettings.RealTimeCPUCoresPercentage);
    SimulationSettings.rayBatchSize = 1;
    SimulationSettings.numVisSamples = SteamAudioSettings.BakingVisibilitySamples;
    SimulationSettings.samplingRate = AudioSettings.samplingRate;
    SimulationSettings.frameSize = AudioSettings.frameSize;
    SimulationSettings.openCLDevice = OpenCLDevice;
    SimulationSettings.radeonRaysDevice = RadeonRaysDevice;
    SimulationSettings.tanDevice = TrueAudioNextDevice;

    return SimulationSettings;
}

IPLSimulationSettings FSteamAudioManager::GetBakingSettings(IPLSimulationFlags Flags)
{
    check(bSettingsLoaded);

    IPLAudioSettings AudioSettings{};
    IAudioEngineState* AudioEngineState = FSteamAudioModule::GetAudioEngineState();
    if (AudioEngineState)
    {
        AudioSettings = AudioEngineState->GetAudioSettings();
    }

    IPLSimulationSettings SimulationSettings{};
    SimulationSettings.flags = Flags;
    SimulationSettings.sceneType = ActualSceneType;
    SimulationSettings.reflectionType = ActualReflectionEffectType;
    SimulationSettings.maxNumOcclusionSamples = SteamAudioSettings.MaxOcclusionSamples;
    SimulationSettings.maxNumRays = SteamAudioSettings.BakingRays;
    SimulationSettings.numDiffuseSamples = 32;
    SimulationSettings.maxDuration = SteamAudioSettings.BakingDuration;
    SimulationSettings.maxOrder = SteamAudioSettings.BakingAmbisonicOrder;
    SimulationSettings.maxNumSources = SteamAudioSettings.RealTimeMaxSources;
    SimulationSettings.numThreads = GetNumThreadsForCPUCoresPercentage(SteamAudioSettings.BakingCPUCoresPercentage);
    SimulationSettings.rayBatchSize = 1;
    SimulationSettings.numVisSamples = SteamAudioSettings.BakingVisibilitySamples;
    SimulationSettings.samplingRate = AudioSettings.samplingRate;
    SimulationSettings.frameSize = AudioSettings.frameSize;
    SimulationSettings.openCLDevice = OpenCLDevice;
    SimulationSettings.radeonRaysDevice = RadeonRaysDevice;
    SimulationSettings.tanDevice = TrueAudioNextDevice;

    return SimulationSettings;
}

IPLInstancedMesh FSteamAudioManager::LoadDynamicObject(USteamAudioDynamicObjectComponent* DynamicObjectComponent)
{
    check(DynamicObjectComponent);

    if (!bInitializationSucceded)
        return nullptr;

    if (!DynamicObjectComponent->GetAssetToLoad().IsAsset())
        return nullptr;

    FString AssetName = DynamicObjectComponent->GetAssetToLoad().GetAssetPathString();

    IPLScene SubScene = nullptr;
    IPLerror Status = IPL_STATUS_SUCCESS;
    if (DynamicObjects.Contains(AssetName))
    {
        SubScene = DynamicObjects[AssetName];
        DynamicObjectRefCounts[AssetName]++;
    }
    else
    {
        IPLSceneSettings SceneSettings{};
        SceneSettings.type = static_cast<IPLSceneType>(ActualSceneType);
        SceneSettings.embreeDevice = EmbreeDevice;
        SceneSettings.radeonRaysDevice = RadeonRaysDevice;

        Status = iplSceneCreate(Context, &SceneSettings, &SubScene);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create scene. [%d]"), Status);
            return nullptr;
        }

        IPLStaticMesh StaticMesh = LoadStaticMeshFromAsset(DynamicObjectComponent->GetAssetToLoad(), Context, SubScene);
        if (!StaticMesh)
        {
            iplSceneRelease(&SubScene);
            return nullptr;
        }

        iplStaticMeshAdd(StaticMesh, SubScene);
        iplSceneCommit(SubScene);

        iplStaticMeshRelease(&StaticMesh);

        DynamicObjects.Add(AssetName, SubScene);
        DynamicObjectRefCounts.Add(AssetName, 1);
    }

    IPLInstancedMeshSettings InstancedMeshSettings{};
    InstancedMeshSettings.subScene = SubScene;
    InstancedMeshSettings.transform = ConvertTransform(DynamicObjectComponent->GetOwner()->GetRootComponent()->GetComponentTransform());

    IPLInstancedMesh InstancedMesh = nullptr;
    Status = iplInstancedMeshCreate(Scene, &InstancedMeshSettings, &InstancedMesh);
    if (Status != IPL_STATUS_SUCCESS)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create instanced mesh. [%d]"), Status);
        return nullptr;
    }

    return InstancedMesh;
}

void FSteamAudioManager::UnloadDynamicObject(USteamAudioDynamicObjectComponent* DynamicObjectComponent)
{
    check(DynamicObjectComponent);

    if (!DynamicObjectComponent->GetAssetToLoad().IsAsset())
        return;

    FString AssetName = DynamicObjectComponent->GetAssetToLoad().GetAssetPathString();

    if (DynamicObjects.Contains(AssetName))
    {
        DynamicObjectRefCounts[AssetName]--;
        if (DynamicObjectRefCounts[AssetName] <= 0)
        {
            iplSceneRelease(&DynamicObjects[AssetName]);

            DynamicObjectRefCounts.Remove(AssetName);
            DynamicObjects.Remove(AssetName);
        }
    }
}

void FSteamAudioManager::AddSource(USteamAudioSourceComponent* Source)
{
    check(Source);
    Sources.Add(Source);
}

void FSteamAudioManager::RemoveSource(USteamAudioSourceComponent* Source)
{
    check(Source);
    Sources.Remove(Source);
}

void FSteamAudioManager::AddListener(USteamAudioListenerComponent* Listener)
{
    check(Listener);
    Listeners.Add(Listener);
}

void FSteamAudioManager::RemoveListener(USteamAudioListenerComponent* Listener)
{
    check(Listener);
    Listeners.Remove(Listener);
}

TStatId FSteamAudioManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(FSteamAudioManager, STATGROUP_Tickables);
}

void FSteamAudioManager::Tick(float DeltaTime)
{
    if (!InitializeSteamAudio(EManagerInitReason::PLAYING))
        return;

    if (ThreadPool && ThreadPoolIdle)
    {
        iplSceneCommit(Scene);

        iplSimulatorSetScene(Simulator, Scene);
        iplSimulatorCommit(Simulator);
    }

    IPLSimulationSettings SimulationSettings = GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

    IPLSimulationSharedInputs SharedInputs{};
    SharedInputs.listener = GetListenerCoordinates();
	SharedInputs.numRays = SimulationSettings.maxNumRays;
	SharedInputs.numBounces = SteamAudioSettings.RealTimeBounces;
	SharedInputs.duration = SimulationSettings.maxDuration;
	SharedInputs.order = SimulationSettings.maxOrder;
	SharedInputs.irradianceMinDistance = SteamAudioSettings.RealTimeIrradianceMinDistance;

    iplSimulatorSetSharedInputs(Simulator, IPL_SIMULATIONFLAGS_DIRECT, &SharedInputs);

    for (USteamAudioSourceComponent* Source : Sources)
    {
        Source->SetInputs(IPL_SIMULATIONFLAGS_DIRECT);
    }

    iplSimulatorRunDirect(Simulator);

    for (USteamAudioSourceComponent* Source : Sources)
    {
        Source->UpdateOutputs(IPL_SIMULATIONFLAGS_DIRECT);
    }

    SimulationUpdateTimeElapsed += DeltaTime;
    if (SimulationUpdateTimeElapsed < SteamAudioSettings.SimulationUpdateInterval)
        return;

    if (ThreadPool && ThreadPoolIdle)
    {
        for (USteamAudioSourceComponent* Source : Sources)
        {
            Source->UpdateOutputs(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));
        }

        for (USteamAudioListenerComponent* Listener : Listeners)
        {
            Listener->UpdateOutputs();
        }

        iplSimulatorSetSharedInputs(Simulator, static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING), &SharedInputs);

        for (USteamAudioSourceComponent* Source : Sources)
        {
            Source->SetInputs(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));
        }

        for (USteamAudioListenerComponent* Listener : Listeners)
        {
            Listener->SetInputs();
        }

        ThreadPoolIdle = false;

        AsyncPool(*ThreadPool, [this]
        {
            iplSimulatorRunReflections(Simulator);
            iplSimulatorRunPathing(Simulator);
            ThreadPoolIdle = true;
        });
    }
}

void FSteamAudioManager::LogCallback(IPLLogLevel Level, IPLstring Message)
{
    FString MessageString(Message);

    if (Level == IPL_LOGLEVEL_ERROR)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("%s"), *MessageString);
    }
    else if (Level == IPL_LOGLEVEL_WARNING)
    {
        UE_LOG(LogSteamAudio, Warning, TEXT("%s"), *MessageString);
    }
    else
    {
        UE_LOG(LogSteamAudio, Log, TEXT("%s"), *MessageString);
    }
}

void* FSteamAudioManager::AllocateCallback(IPLsize Size, IPLsize Alignment)
{
    return FMemory::Malloc(Size, Alignment);
}

void FSteamAudioManager::FreeCallback(void* Ptr)
{
    FMemory::Free(Ptr);
}

}
