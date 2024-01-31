//
// Copyright 2017-2023 Valve Corporation.
//

#include "SteamAudioBakedListenerComponentVisualizer.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "SteamAudioBakedListenerComponent.h"
#include "SteamAudioCommon.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioBakedListenerComponentVisualizer
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioBakedListenerComponentVisualizer::DrawVisualization(const UActorComponent* Component,
    const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    const USteamAudioBakedListenerComponent* BakedListenerComponent = Cast<USteamAudioBakedListenerComponent>(Component);
    if (BakedListenerComponent)
    {
        DrawWireSphere(PDI, BakedListenerComponent->GetOwner()->GetActorLocation(), FLinearColor::Yellow,
            ConvertSteamAudioDistanceToUnreal(BakedListenerComponent->InfluenceRadius), 32, 1);
    }
}

}
