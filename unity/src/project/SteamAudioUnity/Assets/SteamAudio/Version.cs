//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

namespace SteamAudio
{
    public static class Version 
    {
        public const int major = 2;
        public const int minor = 0;
        public const int patch = 16;

        public static int GetVersion() 
        {
            return (major << 16) | (minor << 8) | patch;
        }
    }
}
