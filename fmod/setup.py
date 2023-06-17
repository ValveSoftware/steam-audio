#
# Copyright (c) Valve Corporation. All rights reserved.
#
import os
import sys
import shutil
sys.path.insert(1, "../")

import commons

TargetDirectories = {
    "Win32" : "lib/windows-x86",
    "Win64" : "lib/windows-x64",
    "Linux32" : "lib/linux-x86",
    "Linux64" : "lib/linux-x64",
    "macOS" : "lib/osx",
    "AndroidArmv7" : "lib/android-armv7",
    "AndroidArm64" : "lib/android-armv8",
    "Androidx86" : "lib/android-x86"
    };

argParsed = commons.getDefaultParser("FMOD","").parse_args()

rootPath = commons.download_steamaudioblobs(argParsed.force)

commons.CopyFiles(rootPath, TargetDirectories)

print("Cleaning up...")
shutil.rmtree(rootPath)