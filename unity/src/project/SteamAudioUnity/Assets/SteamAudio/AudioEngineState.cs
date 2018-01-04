//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

namespace SteamAudio
{
    public abstract class AudioEngineState
    {
        public virtual void Initialize(ComponentCache componentCache, GameEngineState gameEngineState)
        {}

        public virtual void Destroy()
        {}

        public virtual void UpdateListener(Vector3 position, Vector3 ahead, Vector3 up)
        {}
    }
}