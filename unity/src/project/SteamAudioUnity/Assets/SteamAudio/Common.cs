//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

namespace SteamAudio
{
    //
    // Basic types.
    //

    // Boolean values.
    public enum Bool
    {
        False,
        True
    }

    // Error codes.
    public enum Error
    {
        None,
        Fail,
        OutOfMemory,
        Initialization
    }

    public delegate void LogCallback(string message);


    //
    // Geometric types.
    //

    // Point in 3D space.
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

    // Box for Acoustic Probes.
    [StructLayout(LayoutKind.Sequential)]
    public struct Box
    {
        public Vector3 minCoordinates;
        public Vector3 maxCoordinates;
    }

    // Sphere for Acoustic Probes.
    [StructLayout(LayoutKind.Sequential)]
    public struct Sphere
    {
        public float centerx;
        public float centery;
        public float centerz;
        public float radius;
    }

    //
    // Audio pipeline.
    //

    // Supported channel layout types.
    public enum ChannelLayoutType
    {
        Speakers,
        Ambisonics
    }

    // Supported channel layouts.
    public enum ChannelLayout
    {
        Mono,
        Stereo,
        Quadraphonic,
        FivePointOne,
        SevenPointOne,
        Custom
    }

    // Supported channel order.
    public enum ChannelOrder
    {
        Interleaved,
        Deinterleaved
    }

    // Supported Ambisonics ordering.
    public enum AmbisonicsOrdering
    {
        FurseMalham,
        ACN
    }

    // Supported Ambisonics normalization.
    public enum AmbisonicsNormalization
    {
        FurseMalham,
        SN3D,
        N3D
    }

    // Audio format.
    [StructLayout(LayoutKind.Sequential)]
    public struct AudioFormat
    {
        public ChannelLayoutType channelLayoutType;
        public ChannelLayout channelLayout;
        public int numSpeakers;
        public Vector3[] speakerDirections;
        public int ambisonicsOrder;
        public AmbisonicsOrdering ambisonicsOrdering;
        public AmbisonicsNormalization ambisonicsNormalization;
        public ChannelOrder channelOrder;
    }

    // Audio format.
    [StructLayout(LayoutKind.Sequential)]
    public struct AudioBuffer
    {
        public AudioFormat audioFormat;
        public int numSamples;
        public float[] interleavedBuffer;
        public IntPtr[] deInterleavedBuffer;
    }

    // Rendering Setings
    [StructLayout(LayoutKind.Sequential)]
    public struct RenderingSettings
    {
        public int samplingRate;
        public int frameSize;
        public ConvolutionOption convolutionOption;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Complex
    {
        public float real;
        public float imag;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void FFTHelper(IntPtr data, IntPtr signal, IntPtr spectrum);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void HRTFLoadCallback(int signalSize, int spectrumSize, FFTHelper fftHelper, IntPtr data);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void HRTFUnloadCallback();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void HRTFLookupCallback(IntPtr direction, IntPtr leftHrtf, IntPtr rightHrtf);

    public enum HRTFDatabaseType
    {
        Default,
        SOFA
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct HRTFParams
    {
        public HRTFDatabaseType type;
        public IntPtr hrtfData;
        public string sofaFileName;
    }

    // HRTF interpolation options.
    public enum HRTFInterpolation
    {
        Nearest,
        Bilinear
    }

    // Indexed triangle.
    [StructLayout(LayoutKind.Sequential)]
    public struct Triangle
    {
        public int index0;
        public int index1;
        public int index2;
    }

    // Material.
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

    // Choose a scene type.
    public enum SceneType
    {
        Phonon,
        Embree,
        RadeonRays,
        Custom
    }

    // Choose a convolution option.
    public enum ConvolutionOption
    {
        Phonon,
        TrueAudioNext
    }

    // Choose an occlusion algorithm for direct sound.
    public enum OcclusionMethod
    {
        Raycast,
        Partial
    }

    // Choose an occlusion algorithm for direct sound.
    public enum OcclusionMode
    {
        NoOcclusion,
        OcclusionWithNoTransmission,
        OcclusionWithFrequencyIndependentTransmission,
        OcclusionWithFrequencyDependentTransmission
    }

    public enum SimulationType
    {
        Realtime,
        Baked
    }

    // Settings for indirect simulation.
    [StructLayout(LayoutKind.Sequential)]
    public struct SimulationSettings
    {
        public SceneType sceneType;
        public int occlusionSamples;
        public int rays;
        public int secondaryRays;
        public int bounces;
        public int threads;
        public float irDuration;
        public int ambisonicsOrder;
        public int maxConvolutionSources;
        public int bakingBatchSize;
        public float irradianceMinDistance;
    }

    // Choose a compute device.
    public enum ComputeDeviceType
    {
        CPU,
        GPU,
        Any
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ComputeDeviceFilter
    {
        public ComputeDeviceType    type;
        public int                  maxCUsToReserve;
        public float                fractionCUsForIRUpdate;
    }

    // Choose probe batch type.
    public enum ProbeBatchType
    {
        Static,
        Dynamic
    }

    // Choose probe placement strategy.
    public enum ProbePlacementStrategy
    {
        Centroid,
        Octree,
        UniformFloor
    }

    // Parameters for probe placement.
    [StructLayout(LayoutKind.Sequential)]
    public struct ProbePlacementParameters
    {
        public ProbePlacementStrategy placement;
        public float horizontalSpacing;
        public float heightAboveFloor;
        public int maxNumTriangles;
        public int maxOctreeDepth;
    }

    // Baking Settings.
    [StructLayout(LayoutKind.Sequential)]
    public struct BakingSettings
    {
        public Bool bakeParametric;
        public Bool bakeConvolution;
    }

    public enum BakedDataType
    {
        StaticSource,
        StaticListener,
        Reverb
    }

    [StructLayout(LayoutKind.Sequential)]
    [Serializable]
    public struct BakedDataIdentifier
    {
        public int              identifier;
        public BakedDataType    type;
    }

    // Settings for propagation rendering.
    [StructLayout(LayoutKind.Sequential)]
    public struct DirectSoundPath
    {
        public Vector3 direction;
        public float distanceAttenuation;
        public float airAbsorptionLow;
        public float airAbsorptionMid;
        public float airAbsorptionHigh;
        public float propagationDelay;
        public float occlusionFactor;
        public float transmissionFactorLow;
        public float transmissionFactorMid;
        public float transmissionFactorHigh;
        public float directivityFactor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Directivity
    {
        public float dipoleWeight;
        public float dipolePower;
        public IntPtr callback;
        public IntPtr userData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Source
    {
        public Vector3 position;
        public Vector3 ahead;
        public Vector3 up;
        public Vector3 right;
        public Directivity directivity;
    }

    // Direct Sound Effect options.
    [StructLayout(LayoutKind.Sequential)]
    public struct DirectSoundEffectOptions
    {
        public Bool applyDistanceAttenuation;
        public Bool applyAirAbsorption;
        public OcclusionMode occlusionMode;
    }

    //
    // Common API functions.
    //
    public static class Common
    {
        public static Vector3 ConvertVector(UnityEngine.Vector3 point)
        {
            Vector3 convertedPoint;
            convertedPoint.x = point.x;
            convertedPoint.y = point.y;
            convertedPoint.z = -point.z;

            return convertedPoint;
        }

        public static UnityEngine.Vector3 ConvertVector(Vector3 point)
        {
            UnityEngine.Vector3 convertedPoint;
            convertedPoint.x = point.x;
            convertedPoint.y = point.y;
            convertedPoint.z = -point.z;

            return convertedPoint;
        }

        public static float[] ConvertMatrix(UnityEngine.Matrix4x4 matrix)
        {
            UnityEngine.Matrix4x4 flipZ = UnityEngine.Matrix4x4.Scale(new UnityEngine.Vector3(1, 1, -1));
            UnityEngine.Matrix4x4 flippedMatrix = flipZ * matrix * flipZ;

            float[] transformArray = new float[16];
            for (int i = 0, count = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j, ++count)
                    transformArray[count] = flippedMatrix[j, i];

            return transformArray;
        }

        public static Matrix4x4 ConvertTransform(Transform transform)
        {
            UnityEngine.Matrix4x4 flipZ = UnityEngine.Matrix4x4.Scale(new UnityEngine.Vector3(1, 1, -1));
            UnityEngine.Matrix4x4 flippedMatrix = flipZ * transform.localToWorldMatrix * flipZ;

            var matrix = new Matrix4x4();
            matrix.m00 = flippedMatrix.m00;
            matrix.m01 = flippedMatrix.m01;
            matrix.m02 = flippedMatrix.m02;
            matrix.m03 = flippedMatrix.m03;
            matrix.m10 = flippedMatrix.m10;
            matrix.m11 = flippedMatrix.m11;
            matrix.m12 = flippedMatrix.m12;
            matrix.m13 = flippedMatrix.m13;
            matrix.m20 = flippedMatrix.m20;
            matrix.m21 = flippedMatrix.m21;
            matrix.m22 = flippedMatrix.m22;
            matrix.m23 = flippedMatrix.m23;
            matrix.m30 = flippedMatrix.m30;
            matrix.m31 = flippedMatrix.m31;
            matrix.m32 = flippedMatrix.m32;
            matrix.m33 = flippedMatrix.m33;

            return matrix;
        }

        public static byte[] ConvertString(string s)
        {
            return Encoding.UTF8.GetBytes(s + Char.MinValue);
        }

        public static int HashForIdentifier(string identifier)
        {
            return BitConverter.ToInt32(MD5.Create().ComputeHash(Encoding.UTF8.GetBytes(identifier)), 0);
        }

        public static byte[] HashStringForIdentifier(string identifier)
        {
            if (identifier == "__reverb__")
                return ConvertString(identifier);
            else
                return ConvertString(HashForIdentifier(identifier).ToString());
        }

        public static byte[] HashStringForIdentifierWithPrefix(string identifier, string prefix)
        {
            if (identifier == "__reverb__")
                return ConvertString(identifier);
            else
                return ConvertString(prefix + HashForIdentifier(identifier).ToString());
        }

        public static string GetStreamingAssetsFileName(string fileName) 
        {
            var streamingAssetsFileName = Path.Combine(Application.streamingAssetsPath, fileName);

#if UNITY_ANDROID && !UNITY_EDITOR
            var tempFileName = Path.Combine(Application.temporaryCachePath, fileName);

            if (File.Exists(tempFileName)) {
                File.Delete(tempFileName);
            }

            try {
                var streamingAssetLoader = new WWW(streamingAssetsFileName);
                while (!streamingAssetLoader.isDone) {
                }

                if (string.IsNullOrEmpty(streamingAssetLoader.error)) {
                    using (var dataWriter = new BinaryWriter(new FileStream(tempFileName, FileMode.Create))) {
                        dataWriter.Write(streamingAssetLoader.bytes);
                        dataWriter.Close();
                    }
                } else {
                    Debug.LogError(streamingAssetLoader.error);
                }
            } catch (Exception exception) {
                Debug.LogError(exception.ToString());
            }

            return tempFileName;
#else
            return streamingAssetsFileName;
#endif
        }
    }
}
