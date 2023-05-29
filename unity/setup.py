#
# Copyright (c) Valve Corporation. All rights reserved.
#

import sys
import shutil
sys.path.insert(1, "../")

import commons

TargetDirectories = {
    "Win32" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86",
    "Win64" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64",
    "Linux32" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86",
    "Linux64" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86_64",
    "macOS" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS",
    "AndroidArmv7" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/armv7",
    "AndroidArm64" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/arm64",
    "Androidx86" : "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/x86"
     };

argParsed = commons.getDefaultParser("Unity", "").parse_args()

rootPath = commons.download_steamaudioblobs(argParsed.force)

commons.CopyFiles(rootPath, TargetDirectories)

print("Cleaning up...")
shutil.rmtree(rootPath)
