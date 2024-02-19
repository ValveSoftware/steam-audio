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
