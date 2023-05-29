#
# Copyright (c) Valve Corporation. All rights reserved.
#

import os
import shutil
import urllib.request, urllib.error, urllib.parse
import zipfile

version = "4.2.0"

def download_file(url):
    remote_file = urllib.request.urlopen(url)
    with open(os.path.basename(url), "wb") as local_file:
        while True:
            data = remote_file.read(1024)
            if not data:
                break
            local_file.write(data)

print("Downloading steamaudio_" + version + ".zip...")
url = "https://github.com/ValveSoftware/steam-audio/releases/download/v" + version + "/steamaudio_" + version + ".zip"
if not os.path.exists("./steamaudio_" + version + ".zip"):
    download_file(url)

print("Extracting steamaudio_" + version + ".zip...")
with zipfile.ZipFile(os.path.basename(url), "r") as zip:
	zip.extractall()

print("Creating directories...")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x86"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x86")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x86"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x86")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x64"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x64")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/osx"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/osx")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/armeabi-v7a"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/armeabi-v7a")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/arm64-v8a"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/arm64-v8a")
if not os.path.exists("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/x86"):
    os.makedirs("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/x86")

print("Copying files...")
shutil.copy("steamaudio/lib/windows-x86/phonon.dll", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x86")
shutil.copy("steamaudio/lib/windows-x86/phonon.lib", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x86")
shutil.copy("steamaudio/lib/windows-x64/phonon.dll", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64")
shutil.copy("steamaudio/lib/windows-x64/phonon.lib", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64")
shutil.copy("steamaudio/lib/linux-x86/libphonon.so", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x86")
shutil.copy("steamaudio/lib/linux-x64/libphonon.so", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x64")
try:
	shutil.rmtree("src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/osx/phonon.bundle")
except:
	pass
shutil.copytree("steamaudio/lib/osx/phonon.bundle", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/osx/phonon.bundle")
shutil.copy("steamaudio/lib/android-armv7/libphonon.so", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/armeabi-v7a")
shutil.copy("steamaudio/lib/android-armv8/libphonon.so", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/arm64-v8a")
shutil.copy("steamaudio/lib/android-x86/libphonon.so", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/x86")

shutil.copy("steamaudio/lib/windows-x64/TrueAudioNext.dll", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64")
shutil.copy("steamaudio/lib/windows-x64/GPUUtilities.dll", "src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64")

print("Cleaning up...")
shutil.rmtree("steamaudio")
