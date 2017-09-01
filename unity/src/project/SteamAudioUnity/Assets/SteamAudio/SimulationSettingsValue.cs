//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    //
    // SimulationSettingsValue
    // The underlying values for a specific set of simulation settings.
    //

    [Serializable]
    public class SimulationSettingsValue
    {

        //
        // Constructor.
        //
        public SimulationSettingsValue()
        {
        }

        //
        // Constructor.
        //
        public SimulationSettingsValue(int realtimeRays, int realtimeSecondaryRays, int realtimeBounces, int bakeRays, int bakeSecondaryRays, int bakeBounces, float duration, int ambisonicsOrder, int maxSources)
        {
            RealtimeRays = realtimeRays;
            RealtimeSecondaryRays = realtimeSecondaryRays;
            RealtimeBounces = realtimeBounces;
            BakeRays = bakeRays;
            BakeSecondaryRays = bakeSecondaryRays;
            BakeBounces = bakeBounces;
            Duration = duration;
            AmbisonicsOrder = ambisonicsOrder;
            MaxSources = maxSources;
        }

        //
        // Copy constructor.
        //
        public SimulationSettingsValue(SimulationSettingsValue other)
        {
            CopyFrom(other);
        }

        //
        // Copies data from another object.
        //
        public void CopyFrom(SimulationSettingsValue other)
        {
            RealtimeRays = other.RealtimeRays;
            RealtimeBounces = other.RealtimeBounces;
            RealtimeSecondaryRays = other.RealtimeSecondaryRays;
            BakeRays = other.BakeRays;
            BakeSecondaryRays = other.BakeSecondaryRays;
            BakeBounces = other.BakeBounces;
            Duration = other.Duration;
            AmbisonicsOrder = other.AmbisonicsOrder;
            MaxSources = other.MaxSources;
        }

        //
        // Data members.
        //

        // Number of rays to trace for realtime simulation.
        [Range(512, 16384)]
        public int RealtimeRays;

        // Number of secondary rays to trace for realtime simulation.
        [Range(128, 4096)]
        public int RealtimeSecondaryRays;

        // Number of bounces to simulate during baking.
        [Range(1, 32)]
        public int RealtimeBounces;

        // Number of rays to trace for baking simulation.
        [Range(8192, 65536)]
        public int BakeRays;

        // Number of secondary rays to trace for baking simulation.
        [Range(1024, 16384)]
        public int BakeSecondaryRays;

        // Number of bounces to simulate during baking.
        [Range(16, 256)]
        public int BakeBounces;

        // Duration of IR.
        [Range(0.1f, 5.0f)]
        public float Duration;

        // Ambisonics order.
        [Range(0, 3)]
        public int AmbisonicsOrder;

        // Maximum number of supported sources.
        [Range(1, 128)]
        public int MaxSources = 32;
    }
}