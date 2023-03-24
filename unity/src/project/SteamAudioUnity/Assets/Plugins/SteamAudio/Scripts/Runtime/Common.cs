//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.IO;
using System.Text;
using UnityEngine;

namespace SteamAudio
{
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

        public static Matrix4x4 TransposeMatrix(Matrix4x4 inMatrix)
        {
            var outMatrix = new Matrix4x4();
            
            outMatrix.m00 = inMatrix.m00;
            outMatrix.m01 = inMatrix.m10;
            outMatrix.m02 = inMatrix.m20;
            outMatrix.m03 = inMatrix.m30;
            outMatrix.m10 = inMatrix.m01;
            outMatrix.m11 = inMatrix.m11;
            outMatrix.m12 = inMatrix.m21;
            outMatrix.m13 = inMatrix.m31;
            outMatrix.m20 = inMatrix.m02;
            outMatrix.m21 = inMatrix.m12;
            outMatrix.m22 = inMatrix.m22;
            outMatrix.m23 = inMatrix.m32;
            outMatrix.m30 = inMatrix.m03;
            outMatrix.m31 = inMatrix.m13;
            outMatrix.m32 = inMatrix.m23;
            outMatrix.m33 = inMatrix.m33;

            return outMatrix;
        }

        public static Matrix4x4 TransformMatrix(UnityEngine.Matrix4x4 inMatrix)
        {
            var outMatrix = new Matrix4x4();

            outMatrix.m00 = inMatrix.m00;
            outMatrix.m01 = inMatrix.m01;
            outMatrix.m02 = inMatrix.m02;
            outMatrix.m03 = inMatrix.m03;
            outMatrix.m10 = inMatrix.m10;
            outMatrix.m11 = inMatrix.m11;
            outMatrix.m12 = inMatrix.m12;
            outMatrix.m13 = inMatrix.m13;
            outMatrix.m20 = inMatrix.m20;
            outMatrix.m21 = inMatrix.m21;
            outMatrix.m22 = inMatrix.m22;
            outMatrix.m23 = inMatrix.m23;
            outMatrix.m30 = inMatrix.m30;
            outMatrix.m31 = inMatrix.m31;
            outMatrix.m32 = inMatrix.m32;
            outMatrix.m33 = inMatrix.m33;

            return outMatrix;
        }

        public static byte[] ConvertString(string s)
        {
            return Encoding.UTF8.GetBytes(s + Char.MinValue);
        }

        public static string GetStreamingAssetsFileName(string fileName)
        {
            var streamingAssetsFileName = Path.Combine(Application.streamingAssetsPath, fileName);

#if UNITY_ANDROID && !UNITY_EDITOR
            var tempFileName = Path.Combine(Application.temporaryCachePath, fileName);

            if (File.Exists(tempFileName))
            {
                File.Delete(tempFileName);
            }

            try
            {
                var streamingAssetLoader = new WWW(streamingAssetsFileName);
                while (!streamingAssetLoader.isDone)
                {
                }

                if (string.IsNullOrEmpty(streamingAssetLoader.error))
                {
                    using (var dataWriter = new BinaryWriter(new FileStream(tempFileName, FileMode.Create)))
                    {
                        dataWriter.Write(streamingAssetLoader.bytes);
                        dataWriter.Close();
                    }
                }
                else
                {
                    Debug.LogError(streamingAssetLoader.error);
                }
            }
            catch (Exception exception)
            {
                Debug.LogError(exception.ToString());
            }

            return tempFileName;
#else
            return streamingAssetsFileName;
#endif
        }

        public static string HumanReadableDataSize(int dataSize)
        {
            if (dataSize < 1e3)
            {
                return dataSize.ToString() + " bytes";
            }
            else if (dataSize < 1e6)
            {
                return (dataSize / 1e3f).ToString("0.0") + " kB";
            }
            else if (dataSize < 1e9)
            {
                return (dataSize / 1e6f).ToString("0.0") + " MB";
            }
            else
            {
                return (dataSize / 1e9f).ToString("0.0") + " GB";
            }
        }
    }
}
