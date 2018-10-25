//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Runtime.InteropServices;

namespace SteamAudio
{
    public static class PhononUnityNative
    {
        [DllImport("audioplugin_phonon")]
        public static extern void iplUnityGetVersion([In, Out] ref uint major,
                                                     [In, Out] ref uint minor,
                                                     [In, Out] ref uint patch);

        [DllImport("audioplugin_phonon")]
        public static extern void iplUnityResetAudioEngine();

        [DllImport("audioplugin_phonon")]
        public static extern int iplUnityAddSOFAFileName(string sofaFileName);

        [DllImport("audioplugin_phonon")]
        public static extern void iplUnityRemoveSOFAFileName(string sofaFileName);

        [DllImport("audioplugin_phonon")]
        public static extern void iplUnitySetCurrentSOFAFile(int index);

        [DllImport("audioplugin_phonon")]
        public static extern void iplUnitySetEnvironment(SimulationSettings simulationSettings, IntPtr environment,
            ConvolutionOption convolutionType);

        [DllImport("audioplugin_phonon")]
        public static extern void iplUnityResetEnvironment();

        [DllImport("audioplugin_phonon")]
        public static extern void iplUnitySetListener(Vector3 position, Vector3 ahead, Vector3 up);
    }
}