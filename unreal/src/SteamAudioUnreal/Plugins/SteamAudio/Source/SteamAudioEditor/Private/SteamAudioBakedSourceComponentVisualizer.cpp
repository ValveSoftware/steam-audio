//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioBakedSourceComponentVisualizer.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "SteamAudioBakedSourceComponent.h"
#include "SteamAudioCommon.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioBakedSourceComponentVisualizer
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioBakedSourceComponentVisualizer::DrawVisualization(const UActorComponent* Component, 
    const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    const USteamAudioBakedSourceComponent* BakedSourceComponent = Cast<USteamAudioBakedSourceComponent>(Component);
    if (BakedSourceComponent)
    {
        DrawWireSphere(PDI, BakedSourceComponent->GetOwner()->GetActorLocation(), FLinearColor::Yellow, 
            ConvertSteamAudioDistanceToUnreal(BakedSourceComponent->InfluenceRadius), 32, 1);
    }
}

}
