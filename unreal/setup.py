#
# Copyright (c) Valve Corporation. All rights reserved.
#

import os
import sys
import shutil
sys.path.insert(1, "../")

import commons

TargetDirectories = {
    "Win32" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x86",
    "Win64" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64",
    "Linux32" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x86",
    "Linux64" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x64",
    "macOS" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/osx",
    "AndroidArmv7" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/armeabi-v7a",
    "AndroidArm64" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/arm64-v8a",
    "Androidx86" : "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/x86"
     };

argParsed = commons.getDefaultParser("Unreal", "").parse_args()

rootPath = commons.download_steamaudioblobs(argParsed.force)

commons.CopyFiles(rootPath, TargetDirectories)

shutil.copy(rootPath + "lib/windows-x86/phonon.lib", TargetDirectories["Win32"])
shutil.copy(rootPath + "lib/windows-x64/phonon.lib", TargetDirectories["Win64"])

print("Cleaning up...")
shutil.rmtree(rootPath)
