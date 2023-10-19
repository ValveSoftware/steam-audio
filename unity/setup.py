#
# Copyright (c) Valve Corporation. All rights reserved.
#

import os
import shutil
import urllib.request, urllib.error, urllib.parse
import zipfile

version = "4.4.1"

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
download_file(url)

print("Extracting steamaudio_" + version + ".zip...")
with zipfile.ZipFile(os.path.basename(url), "r") as zip:
	zip.extractall()

print("Creating directories...")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86_64"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86_64")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/armv7"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/armv7")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/arm64"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/arm64")
if not os.path.exists("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/x86"):
    os.makedirs("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/x86")

print("Copying files...")
shutil.copy("steamaudio/lib/windows-x86/phonon.dll", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86")
shutil.copy("steamaudio/lib/windows-x64/phonon.dll", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64")
shutil.copy("steamaudio/lib/linux-x86/libphonon.so", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86")
shutil.copy("steamaudio/lib/linux-x64/libphonon.so", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86_64")
try:
	shutil.rmtree("src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS/phonon.bundle")
except:
	pass
shutil.copytree("steamaudio/lib/osx/phonon.bundle", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS/phonon.bundle")
shutil.copy("steamaudio/lib/android-armv7/libphonon.so", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/armv7")
shutil.copy("steamaudio/lib/android-armv8/libphonon.so", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/arm64")
shutil.copy("steamaudio/lib/android-x86/libphonon.so", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/x86")

shutil.copy("steamaudio/lib/windows-x64/TrueAudioNext.dll", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64")
shutil.copy("steamaudio/lib/windows-x64/GPUUtilities.dll", "src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64")

print("Cleaning up...")
shutil.rmtree("steamaudio")
