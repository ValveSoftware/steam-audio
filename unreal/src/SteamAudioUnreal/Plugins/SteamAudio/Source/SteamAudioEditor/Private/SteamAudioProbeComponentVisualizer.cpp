//
// Copyright 2017-2023 Valve Corporation.
//

#include "SteamAudioProbeComponentVisualizer.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "SteamAudioProbeComponent.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioProbeComponentVisualizer
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioProbeComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    USteamAudioProbeComponent* ProbeComponent = const_cast<USteamAudioProbeComponent*>(Cast<USteamAudioProbeComponent>(Component));
    if (ProbeComponent)
    {
        FScopeLock Lock(&ProbeComponent->ProbePositionsCriticalSection);
        for (const FVector& Position : ProbeComponent->ProbePositions)
        {
            PDI->DrawPoint(Position, FColor(0, 153, 255), 5, SDPG_World);
        }
    }
}

}
