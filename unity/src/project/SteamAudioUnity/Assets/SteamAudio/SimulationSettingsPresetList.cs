//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

namespace SteamAudio
{
    //
    // SimulationSettingsPresetList
    // A statically-available list of all simulation settings presets and their values.
    //

    public static class SimulationSettingsPresetList
    {

        //
        // Returns whether or not the list has been initialized.
        //
        static bool IsInitialized()
        {
            return (values != null);
        }

        //
        // Initializes the preset list.
        //
        static void Initialize()
        {
            int numPresets = 4;
            values = new SimulationSettingsValue[numPresets];

            values[0] = new SimulationSettingsValue(4096, 1024, 2, 16384, 4096, 32, 1.0f, 1, 32);
            values[1] = new SimulationSettingsValue(8192, 1024, 4, 32768, 4096, 64, 1.0f, 1, 32);
            values[2] = new SimulationSettingsValue(16384, 1024, 8, 65536, 4096, 128, 1.0f, 1, 32);
            values[3] = new SimulationSettingsValue();
        }

        //
        // Returns the value of a given preset by index.
        //
        public static SimulationSettingsValue PresetValue(int index)
        {
            if (!IsInitialized())
                Initialize();

            return values[index];
        }

        //
        // Data members.
        //

        // Array of preset values.
        static SimulationSettingsValue[] values;

    }
}