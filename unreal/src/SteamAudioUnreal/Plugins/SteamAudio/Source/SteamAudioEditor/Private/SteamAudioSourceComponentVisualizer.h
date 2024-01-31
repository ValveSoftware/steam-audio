//
// Copyright 2017-2023 Valve Corporation.
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
