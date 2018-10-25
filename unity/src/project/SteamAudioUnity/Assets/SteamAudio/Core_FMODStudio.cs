//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Runtime.InteropServices;

namespace SteamAudio
{
    public static class PhononFmod
    {
        [DllImport("phonon_fmod")]
        public static extern void iplFmodResetAudioEngine();

        [DllImport("phonon_fmod")]
        public static extern void iplFmodSetEnvironment(SimulationSettings simulationSettings, IntPtr environment,
            ConvolutionOption convolutionType);

		[DllImport("phonon_fmod")]
		public static extern int iplFmodAddSOFAFileName(string sofaFileName);

		[DllImport("phonon_fmod")]
		public static extern void iplFmodSetCurrentSOFAFile(int index);

		[DllImport("phonon_fmod")]
		public static extern void iplFmodRemoveSOFAFileName(string sofaFileName);

		[DllImport("phonon_fmod")]
		public static extern void iplFmodResetEnvironment();
	}
}