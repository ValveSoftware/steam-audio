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

#include "SteamAudioSourceComponentVisualizer.h"
#include "DrawDebugHelpers.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "Components/AudioComponent.h"
#include "SteamAudioCommon.h"
#include "SteamAudioOcclusionSettings.h"
#include "SteamAudioSourceComponent.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSourceComponentVisualizer
// ---------------------------------------------------------------------------------------------------------------------

TArray<FVector> FSteamAudioSourceComponentVisualizer::Vertices;
TArray<int32> FSteamAudioSourceComponentVisualizer::Indices;

FSteamAudioSourceComponentVisualizer::FSteamAudioSourceComponentVisualizer()
    : PrevPosition(0.0f, 0.0f, 0.0f)
    , PrevDipoleWeight(0.0f)
    , PrevDipolePower(0.0f)
{}

void FSteamAudioSourceComponentVisualizer::DrawVisualization(const UActorComponent* Component,
    const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    const USteamAudioSourceComponent* SourceComponent = Cast<USteamAudioSourceComponent>(Component);
    if (SourceComponent)
    {
        const UAudioComponent* AudioComponent = SourceComponent->GetOwner()->FindComponentByClass<UAudioComponent>();
        if (AudioComponent && AudioComponent->AttenuationOverrides.bEnableOcclusion)
        {
            const TArray<UOcclusionPluginSourceSettingsBase*>& OcclusionPluginSettingsArray = AudioComponent->AttenuationOverrides.PluginSettings.OcclusionPluginSettingsArray;
            for (int i = 0; i < OcclusionPluginSettingsArray.Num(); ++i)
            {
                USteamAudioOcclusionSettings* OcclusionPluginSettings = Cast<USteamAudioOcclusionSettings>(OcclusionPluginSettingsArray[i]);
                if (OcclusionPluginSettings && OcclusionPluginSettings->bApplyDirectivity)
                {
                    FVector Position = SourceComponent->GetOwner()->GetActorLocation();
                    float DipoleWeight = OcclusionPluginSettings->DipoleWeight;
                    float DipolePower = OcclusionPluginSettings->DipolePower;

                    if (Position != PrevPosition || DipoleWeight != PrevDipoleWeight || DipolePower != PrevDipolePower)
                    {
                        UpdateDeformedSphereMesh(Position, DipoleWeight, DipolePower);

                        PrevPosition = Position;
                        PrevDipoleWeight = DipoleWeight;
                        PrevDipolePower = DipolePower;
                    }

                    DrawDeformedSphere(PDI);
                    break;
                }
            }
        }
    }
}

void FSteamAudioSourceComponentVisualizer::UpdateDeformedSphereMesh(const FVector& ActorPosition, float DipoleWeight, float DipolePower)
{
    if (Vertices.Num() == 0 || Indices.Num() == 0)
    {
        InitializeSphereMesh(32, 32);
    }

    if (DeformedVertices.Num() == 0)
    {
        DeformedVertices.SetNum(Vertices.Num());
    }

    for (int i = 0; i < Vertices.Num(); ++i)
    {
        DeformedVertices[i] = DeformVertex(Vertices[i], ActorPosition, DipoleWeight, DipolePower);
    }
}

FVector FSteamAudioSourceComponentVisualizer::DeformVertex(const FVector& In, const FVector& ActorPosition, float DipoleWeight, float DipolePower)
{
    float Cosine = In.X;
    float R = 100.0f * FMath::Pow(FMath::Abs((1.0f - DipoleWeight) + DipoleWeight * Cosine), DipolePower);
    return R * In + ActorPosition;
}

void FSteamAudioSourceComponentVisualizer::DrawDeformedSphere(FPrimitiveDrawInterface* PDI)
{
    int NumTriangles = Indices.Num() / 3;
    for (int i = 0, index = 0; i < NumTriangles; ++i)
    {
        int32 Index0 = Indices[index++];
        int32 Index1 = Indices[index++];
        int32 Index2 = Indices[index++];

        const FVector& Vertex0 = DeformedVertices[Index0];
        const FVector& Vertex1 = DeformedVertices[Index1];
        const FVector& Vertex2 = DeformedVertices[Index2];

        if (Index0 < Index1)
            PDI->DrawLine(Vertex0, Vertex1, FLinearColor::Red, SDPG_World);
        if (Index1 < Index2)
            PDI->DrawLine(Vertex1, Vertex2, FLinearColor::Red, SDPG_World);
        if (Index2 < Index0)
            PDI->DrawLine(Vertex2, Vertex0, FLinearColor::Red, SDPG_World);
    }
}

void FSteamAudioSourceComponentVisualizer::InitializeSphereMesh(int NumPhi, int NumTheta)
{
    float DeltaPhi = (2.0f * PI) / NumPhi;
    float DeltaTheta = PI / NumTheta;

    Vertices.SetNum(NumPhi * NumTheta);
    for (int i = 0, Index = 0; i < NumPhi; ++i)
    {
        float Phi = i * DeltaPhi;
        for (int j = 0; j < NumTheta; ++j)
        {
            float Theta = (j * DeltaTheta) - (0.5f * PI);

            float X = FMath::Cos(Theta) * FMath::Sin(Phi);
            float Y = FMath::Sin(Theta);
            float Z = FMath::Cos(Theta) * -FMath::Cos(Phi);

            FVector Vertex = FVector(X, Y, Z);

            Vertices[Index++] = Vertex;
        }
    }

    Indices.SetNum(6 * NumPhi * (NumTheta - 1));
    for (int i = 0, Index = 0; i < NumPhi; ++i)
    {
        for (int j = 0; j < NumTheta - 1; ++j)
        {
            int Index0 = i * NumTheta + j;
            int Index1 = i * NumTheta + (j + 1);
            int Index2 = ((i + 1) % NumPhi) * NumTheta + (j + 1);
            int Index3 = ((i + 1) % NumPhi) * NumTheta + j;

            Indices[Index++] = Index0;
            Indices[Index++] = Index1;
            Indices[Index++] = Index2;
            Indices[Index++] = Index0;
            Indices[Index++] = Index2;
            Indices[Index++] = Index3;
        }
    }
}

}
