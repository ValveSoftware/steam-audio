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

#include "SteamAudioCommon.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// Helper Functions
// ---------------------------------------------------------------------------------------------------------------------

const float SCALEFACTOR = 0.01f;

float ConvertDbToLinear(float dbGain)
{
    const float MIN_DB_LEVEL = -90.0f;

    if (dbGain <= MIN_DB_LEVEL)
        return 0.0f;

    return FPlatformMath::Pow(10.0f, dbGain / 20.0f);
}

float ConvertSteamAudioDistanceToUnreal(float Distance)
{
    return Distance / SCALEFACTOR;
}

IPLVector3 ConvertVector(const FVector& UnrealCoords, bool bScale /* = true */)
{
    IPLVector3 SteamAudioCoords;
    SteamAudioCoords.x = UnrealCoords.Y;
    SteamAudioCoords.y = UnrealCoords.Z;
    SteamAudioCoords.z = -UnrealCoords.X;

    if (bScale)
    {
        SteamAudioCoords.x *= SCALEFACTOR;
        SteamAudioCoords.y *= SCALEFACTOR;
        SteamAudioCoords.z *= SCALEFACTOR;
    }

    return SteamAudioCoords;
}

FVector ConvertVectorInverse(const IPLVector3& SteamAudioCoords, bool bScale /* = true */)
{
	FVector UnrealCoords;
	UnrealCoords.X = -SteamAudioCoords.z;
	UnrealCoords.Y = SteamAudioCoords.x;
	UnrealCoords.Z = SteamAudioCoords.y;

	if (bScale)
	{
		UnrealCoords.X /= SCALEFACTOR;
		UnrealCoords.Y /= SCALEFACTOR;
		UnrealCoords.Z /= SCALEFACTOR;
	}

	return UnrealCoords;
}

IPLMatrix4x4 ConvertTransform(const FTransform& UnrealTransform, bool bRowMajor /* = true */, bool bScale /* = true */)
{
    IPLMatrix4x4 Matrix;

    IPLVector3 Translation = ConvertVector(UnrealTransform.GetTranslation());
    IPLVector3 Scale = ConvertVector(UnrealTransform.GetScale3D(), false);

    FQuat RotationQuatConverted;
    RotationQuatConverted.X = -UnrealTransform.GetRotation().Y;
    RotationQuatConverted.Y = -UnrealTransform.GetRotation().Z;
    RotationQuatConverted.Z = UnrealTransform.GetRotation().X;
    RotationQuatConverted.W = UnrealTransform.GetRotation().W;

    FQuatRotationTranslationMatrix RotationTranslationMatrix(RotationQuatConverted, FVector(Translation.x, Translation.y, Translation.z));
    FScaleMatrix ScaleMatrix(FVector(Scale.x, Scale.y, Scale.z));
    FMatrix ConvertedMatrix = (ScaleMatrix * RotationTranslationMatrix).GetTransposed();

    // Convert to column major if bOutputRowMajor is false, otherwise output column major.
    if (bRowMajor)
    {
        // Row major order (flatten)
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                Matrix.elements[i][j] = ConvertedMatrix.M[i][j];
            }
        }
    }
    else
    {
        // Column major order (conversion from row major order)
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                Matrix.elements[i][j] = ConvertedMatrix.M[j][i];
            }
        }
    }

    return Matrix;
}

int CalcIRSizeForDuration(float Duration, int SamplingRate)
{
    check(Duration > 0.0f);
    check(SamplingRate > 0);

    return static_cast<int>(ceilf(Duration * SamplingRate));
}

int CalcNumChannelsForAmbisonicOrder(int Order)
{
    check(Order >= 0);

    return (Order + 1) * (Order + 1);
}

IPLSpeakerLayout GetSpeakerLayoutForNumChannels(int NumChannels)
{
    IPLSpeakerLayout SpeakerLayout{};

    switch (NumChannels)
    {
    case 1:
        SpeakerLayout.type = IPL_SPEAKERLAYOUTTYPE_MONO;
        break;
    case 2:
        SpeakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
        break;
    case 4:
        SpeakerLayout.type = IPL_SPEAKERLAYOUTTYPE_QUADRAPHONIC;
        break;
    case 6:
        SpeakerLayout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_5_1;
        break;
    case 8:
        SpeakerLayout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_7_1;
        break;
    default:
        break;
    }

    return SpeakerLayout;
}

int GetNumThreadsForCPUCoresPercentage(float Percentage)
{
    check(0.0f <= Percentage && Percentage <= 100.0f);

    int NumLogicalCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    return FPlatformMath::Max(0, FPlatformMath::Min(FPlatformMath::CeilToInt((Percentage / 100.0f) * NumLogicalCores), NumLogicalCores));
}

}
