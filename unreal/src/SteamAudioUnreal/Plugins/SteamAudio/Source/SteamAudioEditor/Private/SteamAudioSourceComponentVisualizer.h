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

#include "SteamAudioEditorModule.h"
#include "ComponentVisualizer.h"

class FPrimitiveDrawInterface;
class FSceneView;
class USteamAudioSourceComponent;

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSourceComponentVisualizer
// ---------------------------------------------------------------------------------------------------------------------

class FSteamAudioSourceComponentVisualizer : public FComponentVisualizer
{
public:
    FSteamAudioSourceComponentVisualizer();

    virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

private:
    TArray<FVector> DeformedVertices;
    FVector PrevPosition;
    float PrevDipoleWeight;
    float PrevDipolePower;

    static TArray<FVector> Vertices;
    static TArray<int32> Indices;

    void UpdateDeformedSphereMesh(const FVector& ActorPosition, float DipoleWeight, float DipolePower);
    FVector DeformVertex(const FVector& In, const FVector& ActorPosition, float DipoleWeight, float DipolePower);
    void DrawDeformedSphere(FPrimitiveDrawInterface* PDI);

    static void InitializeSphereMesh(int NumPhi, int NumTheta);
};

}
