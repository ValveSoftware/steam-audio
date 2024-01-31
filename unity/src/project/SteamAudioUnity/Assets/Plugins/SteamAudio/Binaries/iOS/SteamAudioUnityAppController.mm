//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#import "SteamAudioUnityAppController.h"

extern "C" {

struct UnityAudioEffectDefinition;

extern void UnityRegisterAudioPlugin(int (*)(UnityAudioEffectDefinition***));
extern int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition***);

}

@implementation SteamAudioUnityAppController

- (void) preStartUnity
{
    [super preStartUnity];
    UnityRegisterAudioPlugin(UnityGetAudioEffectDefinitions);
}

@end

IMPL_APP_CONTROLLER_SUBCLASS(SteamAudioUnityAppController);
