#
# Copyright (c) Valve Corporation. All rights reserved.
#

import os
import shutil
import urllib.request, urllib.error, urllib.parse
import zipfile
import argparse

import version as SteamAudioVersion

DownloadPath = "../"

MainLibsToCopy = {
    "Win32" : "lib/windows-x86/phonon.dll",
    "Win64" : "lib/windows-x64/phonon.dll",
    "Linux32" : "lib/linux-x86/libphonon.so",
    "Linux64" : "lib/linux-x64/libphonon.so",
    "macOS" : "lib/osx/phonon.bundle",
    "AndroidArmv7" : "lib/android-armv7/libphonon.so",
    "AndroidArm64" : "lib/android-armv8/libphonon.so",
    "Androidx86" : "lib/android-x86/libphonon.so"
     };

def getDefaultParser(name,description):
    parser = argparse.ArgumentParser(prog=name,description=description)
    parser.add_argument('-f', '--force', action='store_true')
    return parser

def download_file(url):
    remote_file = urllib.request.urlopen(url)
    with open(DownloadPath + os.path.basename(url), "wb") as local_file:
        while True:
            data = remote_file.read(1024)
            if not data:
                break
            local_file.write(data)

def download_steamaudioblobs(force):
    url = "https://github.com/ValveSoftware/steam-audio/releases/download/v" + SteamAudioVersion.version + "/steamaudio_" + SteamAudioVersion.version + ".zip"
    
    if force:
        os.remove("./steamaudio_" + SteamAudioVersion.version + ".zip")
    
    if not os.path.exists(DownloadPath + "steamaudio_" + SteamAudioVersion.version + ".zip"):
        print("Downloading steamaudio_" + SteamAudioVersion.version + ".zip...")
        download_file(url)
    else:
        print("steamaudio_" + SteamAudioVersion.version + ".zip, It's already downloaded, skipping... (For avoid this behaviour use -f or --force)")

    print("Extracting steamaudio_" + SteamAudioVersion.version + ".zip...")
    with zipfile.ZipFile( DownloadPath + os.path.basename(url), "r") as zip:
        zip.extractall()
    
    return os.path.realpath("./steamaudio") + "/"

def CopyFiles(rootpath, TargetDirectories, isFMod = 0):
    print("Creating directories...")
    for i in TargetDirectories.values():
        if not os.path.exists(i):
            os.makedirs(i)

    print("Copying files...")
    for key, value in MainLibsToCopy.items():
        if key != "macOS":
            shutil.copy(rootpath + value, TargetDirectories[key])

    try:
        shutil.rmtree(TargetDirectories["macOS"]+"/phonon.bundle")
    except:
        pass
    shutil.copytree(rootpath + MainLibsToCopy["macOS"], TargetDirectories["macOS"]+"/phonon.bundle")
    
    if not isFMod:
        shutil.copy(rootpath + "lib/windows-x64/TrueAudioNext.dll", TargetDirectories["Win64"])
        shutil.copy(rootpath + "lib/windows-x64/GPUUtilities.dll", TargetDirectories["Win64"])
