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
