﻿//
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

using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace SteamAudio
{
    // CONSTANTS

    public static class Constants
    {
        public const uint kVersionMajor = 4;
        public const uint kVersionMinor = 7;
        public const uint kVersionPatch = 0;
        public const uint kVersion = (kVersionMajor << 16) | (kVersionMinor << 8) | kVersionPatch;
    }

    // ENUMERATIONS

    public enum Bool
    {
        False,
        True
    }

    public enum Error
    {
        Success,
        Failure,
        OutOfMemory,
        Initialization
    }

    public enum LogLevel
    {
        Info,
        Warning,
        Error,
        Debug
    }

    public enum SIMDLevel
    {
        SSE2,
        SSE4,
        AVX,
        AVX2,
        AVX512,
        NEON = SSE2
    }

    [Flags]
    public enum ContextFlags
    {
        Validation = 1 << 0,
        Force32Bit = 0x7fffffff
    }

    public enum OpenCLDeviceType
    {
        Any,
        CPU,
        GPU
    }

    public enum SceneType
    {
        Default,
        Embree,
        RadeonRays,
#if UNITY_2019_2_OR_NEWER
        [InspectorName("Unity")]
#endif
        Custom
    }

    public enum HRTFType
    {
        Default,
        SOFA
    }

    public enum HRTFNormType
    {
        None,
        RMS
    }

    public enum ProbeGenerationType
    {
        Centroid,
        UniformFloor
    }

    public enum BakedDataVariation
    {
        Reverb,
        StaticSource,
        StaticListener,
        Dynamic
    }

    public enum BakedDataType
    {
        Reflections,
        Pathing
    }

    [Flags]
    public enum SimulationFlags
    {
        Direct = 1 << 0,
        Reflections = 1 << 1,
        Pathing = 1 << 2
    }

    [Flags]
    public enum DirectSimulationFlags
    {
        DistanceAttenuation = 1 << 0,
        AirAbsorption = 1 << 1,
        Directivity = 1 << 2,
        Occlusion = 1 << 3,
        Transmission = 1 << 4
    }

    public enum HRTFInterpolation
    {
        Nearest,
        Bilinear
    }

    public enum DistanceAttenuationModelType
    {
        Default,
        InverseDistance,
        Callback
    }

    public enum AirAbsorptionModelType
    {
        Default,
        Exponential,
        Callback
    }

    public enum DeviationModelType
    {
        Default,
        Callback
    }

    public enum OcclusionType
    {
        Raycast,
        Volumetric
    }

    [Flags]
    public enum DirectEffectFlags
    {
        ApplyDistanceAttenuation = 1 << 0,
        ApplyAirAbsorption = 1 << 1,
        ApplyDirectivity = 1 << 2,
        ApplyOcclusion = 1 << 3,
        ApplyTransmission = 1 << 4
    }

    public enum TransmissionType
    {
        FrequencyIndependent,
        FrequencyDependent
    }

    public enum ReflectionEffectType
    {
        Convolution,
        Parametric,
        Hybrid,
#if UNITY_2019_2_OR_NEWER
        [InspectorName("TrueAudio Next")]
#endif
        TrueAudioNext
    }

    [Flags]
    public enum ReflectionsBakeFlags
    {
        BakeConvolution = 1 << 0,
        BakeParametric = 1 << 1
    }

    // CALLBACKS

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void ProgressCallback(float progress, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void LogCallback(LogLevel level, string message);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate IntPtr AllocateCallback(UIntPtr size, UIntPtr alignment);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void FreeCallback(IntPtr memoryBlock);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void ClosestHitCallback(ref Ray ray, float minDistance, float maxDistance, out Hit hit, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void AnyHitCallback(ref Ray ray, float minDistance, float maxDistance, out byte occluded, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void BatchedClosestHitCallback(int numRays, Ray[] rays, float[] minDistances, float[] maxDistances, [Out] Hit[] hits, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void BatchedAnyHitCallback(int numRays, Ray[] rays, float[] minDistances, float[] maxDistances, [Out] byte[] occluded, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate float DistanceAttenuationCallback(float distance, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate float AirAbsorptionCallback(float distance, int band, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate float DeviationCallback(float angle, int band, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate float DirectivityCallback(Vector3 direction, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate void PathingVisualizationCallback(Vector3 from, Vector3 to, Bool occluded, IntPtr userData);

    // STRUCTURES

    [StructLayout(LayoutKind.Sequential)]
    public struct ContextSettings
    {
        public uint version;
        public LogCallback logCallback;
        public AllocateCallback allocateCallback;
        public FreeCallback freeCallback;
        public SIMDLevel simdLevel;
        public ContextFlags flags;
    }

    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float x;
        public float y;
        public float z;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix4x4
    {
        public float m00;
        public float m01;
        public float m02;
        public float m03;
        public float m10;
        public float m11;
        public float m12;
        public float m13;
        public float m20;
        public float m21;
        public float m22;
        public float m23;
        public float m30;
        public float m31;
        public float m32;
        public float m33;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Box
    {
        public Vector3 minCoordinates;
        public Vector3 maxCoordinates;
    }

    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Sphere
    {
        public Vector3 center;
        public float radius;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct CoordinateSpace3
    {
        public Vector3 right;
        public Vector3 up;
        public Vector3 ahead;
        public Vector3 origin;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SerializedObjectSettings
    {
        public IntPtr data;
        public UIntPtr size;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EmbreeDeviceSettings { }

    [StructLayout(LayoutKind.Sequential)]
    public struct OpenCLDeviceSettings
    {
        public OpenCLDeviceType type;
        public int numCUsToReserve;
        public float fractionCUsForIRUpdate;
        public Bool requiresTAN;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OpenCLDeviceDesc
    {
        public IntPtr platform;
        public string platformName;
        public string platformVendor;
        public string platformVersion;
        public IntPtr device;
        public string deviceName;
        public string deviceVendor;
        public string deviceVersion;
        public OpenCLDeviceType type;
        public int numConvolutionCUs;
        public int numIRUpdateCUs;
        public int granularity;
        public float perfScore;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RadeonRaysDeviceSettings { }

    [StructLayout(LayoutKind.Sequential)]
    public struct TrueAudioNextDeviceSettings
    {
        public int frameSize;
        public int irSize;
        public int order;
        public int maxSources;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Triangle
    {
        public int index0;
        public int index1;
        public int index2;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Material
    {
        public float absorptionLow;
        public float absorptionMid;
        public float absorptionHigh;
        public float scattering;
        public float transmissionLow;
        public float transmissionMid;
        public float transmissionHigh;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Ray
    {
        public Vector3 origin;
        public Vector3 direction;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Hit
    {
        public float distance;
        public int triangleIndex;
        public int objectIndex;
        public int materialIndex;
        public Vector3 normal;
        public IntPtr material;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SceneSettings
    {
        public SceneType type;
        public ClosestHitCallback closestHitCallback;
        public AnyHitCallback anyHitCallback;
        public BatchedClosestHitCallback batchedClosestHitCallback;
        public BatchedAnyHitCallback batchedAnyHitCallback;
        public IntPtr userData;
        public IntPtr embreeDevice;
        public IntPtr radeonRaysDevice;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StaticMeshSettings
    {
        public int numVertices;
        public int numTriangles;
        public int numMaterials;
        public IntPtr vertices;
        public IntPtr triangles;
        public IntPtr materialIndices;
        public IntPtr materials;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct InstancedMeshSettings
    {
        public IntPtr subScene;
        public Matrix4x4 transform;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AudioSettings
    {
        public int samplingRate;
        public int frameSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct HRTFSettings
    {
        public HRTFType type;
        public string sofaFileName;
        public IntPtr sofaFileData;
        public int sofaFileDataSize;
        public float volume;
        public HRTFNormType normType;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ProbeGenerationParams
    {
        public ProbeGenerationType type;
        public float spacing;
        public float height;
        public Matrix4x4 transform;
    }

    [StructLayout(LayoutKind.Sequential)]
    [Serializable]
    public struct BakedDataIdentifier
    {
        public BakedDataType type;
        public BakedDataVariation variation;
        public Sphere endpointInfluence;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReflectionsBakeParams
    {
        public IntPtr scene;
        public IntPtr probeBatch;
        public SceneType sceneType;
        public BakedDataIdentifier identifier;
        public ReflectionsBakeFlags flags;
        public int numRays;
        public int numDiffuseSamples;
        public int numBounces;
        public float simulatedDuration;
        public float savedDuration;
        public int order;
        public int numThreads;
        public int rayBatchSize;
        public float irradianceMinDistance;
        public int bakeBatchSize;
        public IntPtr openCLDevice;
        public IntPtr radeonRaysDevice;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PathBakeParams
    {
        public IntPtr scene;
        public IntPtr probeBatch;
        public BakedDataIdentifier identifier;
        public int numSamples;
        public float radius;
        public float threshold;
        public float visRange;
        public float pathRange;
        public int numThreads;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DistanceAttenuationModel
    {
        public DistanceAttenuationModelType type;
        public float minDistance;
        public DistanceAttenuationCallback callback;
        public IntPtr userData;
        public Bool dirty;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AirAbsorptionModel
    {
        public AirAbsorptionModelType type;
        public float coefficientsLow;
        public float coefficientsMid;
        public float coefficientsHigh;
        public AirAbsorptionCallback callback;
        public IntPtr userData;
        public Bool dirty;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Directivity
    {
        public float dipoleWeight;
        public float dipolePower;
        public DirectivityCallback callback;
        public IntPtr userData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DeviationModel
    {
        public DeviationModelType type;
        public DeviationCallback callback;
        public IntPtr userData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SimulationSettings
    {
        public SimulationFlags flags;
        public SceneType sceneType;
        public ReflectionEffectType reflectionType;
        public int maxNumOcclusionSamples;
        public int maxNumRays;
        public int numDiffuseSamples;
        public float maxDuration;
        public int maxOrder;
        public int maxNumSources;
        public int numThreads;
        public int rayBatchSize;
        public int numVisSamples;
        public int samplingRate;
        public int frameSize;
        public IntPtr openCLDevice;
        public IntPtr radeonRaysDevice;
        public IntPtr tanDevice;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SourceSettings
    {
        public SimulationFlags flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SimulationInputs
    {
        public SimulationFlags flags;
        public DirectSimulationFlags directFlags;
        public CoordinateSpace3 source;
        public DistanceAttenuationModel distanceAttenuationModel;
        public AirAbsorptionModel airAbsorptionModel;
        public Directivity directivity;
        public OcclusionType occlusionType;
        public float occlusionRadius;
        public int numOcclusionSamples;
        public float reverbScaleLow;
        public float reverbScaleMid;
        public float reverbScaleHigh;
        public float hybridReverbTransitionTime;
        public float hybridReverbOverlapPercent;
        public Bool baked;
        public BakedDataIdentifier bakedDataIdentifier;
        public IntPtr pathingProbes;
        public float visRadius;
        public float visThreshold;
        public float visRange;
        public int pathingOrder;
        public Bool enableValidation;
        public Bool findAlternatePaths;
        public int numTransmissionRays;
        public IntPtr deviationModel;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SimulationSharedInputs
    {
        public CoordinateSpace3 listener;
        public int numRays;
        public int numBounces;
        public float duration;
        public int order;
        public float irradianceMinDistance;
        public PathingVisualizationCallback pathingVisualizationCallback;
        public IntPtr pathingUserData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DirectEffectParams
    {
        public DirectEffectFlags flags;
        public TransmissionType transmissionType;
        public float distanceAttenuation;
        public float airAbsorptionLow;
        public float airAbsorptionMid;
        public float airAbsorptionHigh;
        public float directivity;
        public float occlusion;
        public float transmissionLow;
        public float transmissionMid;
        public float transmissionHigh;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReflectionEffectParams
    {
        public ReflectionEffectType type;
        public IntPtr ir;
        public float reverbTimesLow;
        public float reverbTimesMid;
        public float reverbTimesHigh;
        public float eqLow;
        public float eqMid;
        public float eqHigh;
        public int delay;
        public int numChannels;
        public int irSize;
        public IntPtr tanDevice;
        public int tanSlot;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PathEffectParams
    {
        public float eqCoeffsLow;
        public float eqCoeffsMid;
        public float eqCoeffsHigh;
        public IntPtr shCoeffs;
        public int order;
        public Bool binaural;
        public IntPtr hrtf;
        public CoordinateSpace3 listener;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SimulationOutputs
    {
        public DirectEffectParams direct;
        public ReflectionEffectParams reflections;
        public PathEffectParams pathing;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PerspectiveCorrection
    {
        public Bool enabled;
        public float xfactor;
        public float yfactor;
        public Matrix4x4 transform;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct EnergyFieldSettings
    {
        public float duration;
        public int order;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ImpulseResponseSettings
    {
        public float duration;
        public int order;
        public int samplingRate;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReconstructorSettings
    {
        public float maxDuration;
        public int maxOrder;
        public int samplingRate;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReconstructorInputs
    {
        public IntPtr energyField;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReconstructorSharedInputs
    {
        public float duration;
        public int order;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ReconstructorOutputs
    {
        public IntPtr impulseResponse;
    }

    // FUNCTIONS

    public static class API
    {
        // Context

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplContextCreate(ref ContextSettings settings, out IntPtr context);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplContextRetain(IntPtr context);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplContextRelease(ref IntPtr context);

        // Geometry

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Vector3 iplCalculateRelativeDirection(IntPtr context, Vector3 sourcePosition, Vector3 listenerPosition, Vector3 listenerAhead, Vector3 listenerUp);

        // Serialization

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplSerializedObjectCreate(IntPtr context, ref SerializedObjectSettings settings, out IntPtr serializedObject);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplSerializedObjectRetain(IntPtr serializedObject);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSerializedObjectRelease(ref IntPtr serializedObject);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern UIntPtr iplSerializedObjectGetSize(IntPtr serializedObject);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplSerializedObjectGetData(IntPtr serializedObject);

        // Embree

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplEmbreeDeviceCreate(IntPtr context, ref EmbreeDeviceSettings settings, out IntPtr device);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplEmbreeDeviceRetain(IntPtr device);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEmbreeDeviceRelease(ref IntPtr device);

        // OpenCL

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplOpenCLDeviceListCreate(IntPtr context, ref OpenCLDeviceSettings settings, out IntPtr deviceList);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplOpenCLDeviceListRetain(IntPtr deviceList);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplOpenCLDeviceListRelease(ref IntPtr deviceList);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern int iplOpenCLDeviceListGetNumDevices(IntPtr deviceList);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplOpenCLDeviceListGetDeviceDesc(IntPtr deviceList, int index, out OpenCLDeviceDesc deviceDesc);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplOpenCLDeviceCreate(IntPtr context, IntPtr deviceList, int index, out IntPtr device);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplOpenCLDeviceRetain(IntPtr device);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplOpenCLDeviceRelease(ref IntPtr device);

        // Radeon Rays

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplRadeonRaysDeviceCreate(IntPtr openCLDevice, ref RadeonRaysDeviceSettings settings, out IntPtr rrDevice);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplRadeonRaysDeviceRetain(IntPtr device);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplRadeonRaysDeviceRelease(ref IntPtr device);

        // TrueAudio Next

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplTrueAudioNextDeviceCreate(IntPtr openCLDevice, ref TrueAudioNextDeviceSettings settings, out IntPtr tanDevice);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplTrueAudioNextDeviceRetain(IntPtr device);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplTrueAudioNextDeviceRelease(ref IntPtr device);

        // Scene

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplSceneCreate(IntPtr context, ref SceneSettings settings, out IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplSceneRetain(IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSceneRelease(ref IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplSceneLoad(IntPtr context, ref SceneSettings settings, IntPtr serializedObject, ProgressCallback progressCallback, IntPtr progressCallbackUserData, out IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSceneSave(IntPtr scene, IntPtr serializedObject);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSceneSaveOBJ(IntPtr scene, string fileBaseName);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSceneCommit(IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplStaticMeshCreate(IntPtr scene, ref StaticMeshSettings settings, out IntPtr staticMesh);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplStaticMeshRetain(IntPtr staticMesh);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplStaticMeshRelease(ref IntPtr staticMesh);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplStaticMeshLoad(IntPtr scene, IntPtr serializedObject, ProgressCallback progressCallback, IntPtr progressCallbackUserData, out IntPtr staticMesh);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplStaticMeshSave(IntPtr staticMesh, IntPtr serializedObject);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplStaticMeshAdd(IntPtr staticMesh, IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplStaticMeshRemove(IntPtr staticMesh, IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplInstancedMeshCreate(IntPtr scene, ref InstancedMeshSettings settings, out IntPtr instancedMesh);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplInstancedMeshRetain(IntPtr instancedMesh);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplInstancedMeshRelease(ref IntPtr instancedMesh);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplInstancedMeshAdd(IntPtr instancedMesh, IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplInstancedMeshRemove(IntPtr instancedMesh, IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplInstancedMeshUpdateTransform(IntPtr instancedMesh, IntPtr scene, Matrix4x4 transform);

        // HRTF

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplHRTFCreate(IntPtr context, ref AudioSettings audioSettings, ref HRTFSettings hrtfSettings, out IntPtr hrtf);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplHRTFRetain(IntPtr hrtf);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplHRTFRelease(ref IntPtr hrtf);

        // Probes

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplProbeArrayCreate(IntPtr context, out IntPtr probeArray);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplProbeArrayRetain(IntPtr probeArray);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeArrayRelease(ref IntPtr probeArray);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeArrayGenerateProbes(IntPtr probeArray, IntPtr scene, ref ProbeGenerationParams generationParams);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern int iplProbeArrayGetNumProbes(IntPtr probeArray);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Sphere iplProbeArrayGetProbe(IntPtr probeArray, int index);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplProbeBatchCreate(IntPtr context, out IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplProbeBatchRetain(IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchRelease(ref IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplProbeBatchLoad(IntPtr context, IntPtr serializedObject, out IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchSave(IntPtr probeBatch, IntPtr serializedObject);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern int iplProbeBatchGetNumProbes(IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchAddProbe(IntPtr probeBatch, Sphere probe);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchAddProbeArray(IntPtr probeBatch, IntPtr probeArray);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchRemoveProbe(IntPtr probeBatch, int index);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchCommit(IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchRemoveData(IntPtr probeBatch, ref BakedDataIdentifier identifier);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern UIntPtr iplProbeBatchGetDataSize(IntPtr probeBatch, ref BakedDataIdentifier identifier);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchGetEnergyField(IntPtr probeBatch, ref BakedDataIdentifier identifier, int probeIndex, IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplProbeBatchGetReverb(IntPtr probeBatch, ref BakedDataIdentifier identifier, int probeIndex, float[] reverbTimes);

        // Baking

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplReflectionsBakerBake(IntPtr context, ref ReflectionsBakeParams bakeParams, ProgressCallback progressCallback, IntPtr userData);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplReflectionsBakerCancelBake(IntPtr context);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplPathBakerBake(IntPtr context, ref PathBakeParams bakeParams, ProgressCallback progressCallback, IntPtr userData);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplPathBakerCancelBake(IntPtr context);

        // Run-Time Simulation

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplSimulatorCreate(IntPtr context, ref SimulationSettings settings, out IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplSimulatorRetain(IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorRelease(ref IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorSetScene(IntPtr simulator, IntPtr scene);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorAddProbeBatch(IntPtr simulator, IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorRemoveProbeBatch(IntPtr simulator, IntPtr probeBatch);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorSetSharedInputs(IntPtr simulator, SimulationFlags flags, ref SimulationSharedInputs sharedInputs);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorCommit(IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorRunDirect(IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorRunReflections(IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSimulatorRunPathing(IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplSourceCreate(IntPtr simulator, ref SourceSettings settings, out IntPtr source);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplSourceRetain(IntPtr source);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSourceRelease(ref IntPtr source);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSourceAdd(IntPtr source, IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSourceRemove(IntPtr source, IntPtr simulator);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSourceSetInputs(IntPtr source, SimulationFlags flags,  ref SimulationInputs inputs);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplSourceGetOutputs(IntPtr source, SimulationFlags flags,  ref SimulationOutputs outputs);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern float iplDistanceAttenuationCalculate(IntPtr context, Vector3 source, Vector3 listener, ref DistanceAttenuationModel model);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplAirAbsorptionCalculate(IntPtr context, Vector3 source, Vector3 listener, ref AirAbsorptionModel mode, float[] minDistances);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern float iplDirectivityCalculate(IntPtr context, CoordinateSpace3 source, Vector3 listener, ref Directivity model);

        // Energy Field API

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplEnergyFieldCreate(IntPtr context, ref EnergyFieldSettings settings, out IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplEnergyFieldRetain(IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEnergyFieldRelease(ref IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern int iplEnergyFieldGetNumChannels(IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern int iplEnergyFieldGetNumBins(IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplEnergyFieldGetData(IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplEnergyFieldGetChannel(IntPtr energyField, int channelIndex);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplEnergyFieldGetBand(IntPtr energyField, int channelIndex, int bandIndex);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEnergyFieldReset(IntPtr energyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEnergyFieldCopy(IntPtr srcEnergyField, IntPtr dstEnergyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEnergyFieldSwap(IntPtr energyFieldA, IntPtr energyFieldB);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEnergyFieldAdd(IntPtr energyField1, IntPtr energyField2, IntPtr outEnergyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEnergyFieldScale(IntPtr inEnergyField, float scalar, IntPtr outEnergyField);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplEnergyFieldScaleAccum(IntPtr inEnergyField, float scalar, IntPtr outEnergyField);

        // Impulse Response API

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern Error iplImpulseResponseCreate(IntPtr context, ref ImpulseResponseSettings settings, out IntPtr impulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplImpulseResponseRetain(IntPtr impulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplImpulseResponseRelease(ref IntPtr impulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern int iplImpulseResponseGetNumChannels(IntPtr impulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern int iplImpulseResponseGetNumSamples(IntPtr impulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplImpulseResponseGetData(IntPtr impulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplImpulseResponseGetChannel(IntPtr impulseResponse, int channelIndex);
#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplImpulseResponseReset(IntPtr impulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplImpulseResponseCopy(IntPtr srcImpulseReponse, IntPtr dstImpulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplImpulseResponseSwap(IntPtr impulseResponse1, IntPtr impulseResponse2);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplImpulseResponseAdd(IntPtr inImpulseResponse1, IntPtr inImpulseResponse2, IntPtr outImpulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplImpulseResponseScale(IntPtr inImpulseResponse, float scalar, IntPtr outImpulseResponse);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplImpulseResponseScaleAccum(IntPtr inImpulseRespnse, float scalar, IntPtr outImpulseResponse);

        // Reconstructor API

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]

#endif
        public static extern Error iplReconstructorCreate(IntPtr context, ref ReconstructorSettings settings, out IntPtr reconstructor);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern IntPtr iplReconstructorRetain(IntPtr reconstructor);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplReconstructorRelease(ref IntPtr reconstructor);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("phonon")]
#endif
        public static extern void iplReconstructorReconstruct(IntPtr reconstructor, int numInputs, ref ReconstructorInputs inputs, ref ReconstructorSharedInputs sharedInputs, ref ReconstructorOutputs outputs);

        // UNITY PLUGIN

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void iplUnityInitialize(IntPtr context);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void iplUnitySetPerspectiveCorrection(PerspectiveCorrection correction);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void  iplUnitySetHRTF(IntPtr hrtf);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void iplUnitySetSimulationSettings(SimulationSettings simulationSettings);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void iplUnitySetReverbSource(IntPtr reverbSource);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern int iplUnityAddSource(IntPtr source);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void iplUnityRemoveSource(int handle);

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void iplUnityTerminate();

#if (UNITY_IOS || UNITY_WEBGL) && !UNITY_EDITOR
        [DllImport("__Internal")]
#else
        [DllImport("audioplugin_phonon")]
#endif
        public static extern void iplUnitySetHRTFDisabled(bool disabled);
    }
}
