#
# Copyright (c) Valve Corporation. All rights reserved.
#

import os
import shutil
import urllib2
import zipfile

version = "2.0-beta.15"

def download_file(url):
    remote_file = urllib2.urlopen(url)
    with open(os.path.basename(url), "wb") as local_file:
        while True:
            data = remote_file.read(1024)
            if not data:
                break
            local_file.write(data)

print "Downloading steamaudio_api_" + version + ".zip..."
url = "https://github.com/ValveSoftware/steam-audio/releases/download/v" + version + "/steamaudio_api_" + version + ".zip"
download_file(url)

print "Extracting steamaudio_api_" + version + ".zip..."
with zipfile.ZipFile(os.path.basename(url), "r") as zip:
	zip.extractall()

print "Copying files..."
shutil.copy("steamaudio_api/bin/Windows/x86/phonon.dll", "src/project/SteamAudioUnity/Assets/Plugins/x86")
shutil.copy("steamaudio_api/bin/Windows/x64/phonon.dll", "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_api/lib/Linux/x86/libphonon.so", "src/project/SteamAudioUnity/Assets/Plugins/x86")
shutil.copy("steamaudio_api/lib/Linux/x64/libphonon.so", "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
try:
	shutil.rmtree("src/project/SteamAudioUnity/Assets/Plugins/phonon.bundle")
except:
	pass
shutil.copytree("steamaudio_api/bin/OSX/phonon.bundle",  "src/project/SteamAudioUnity/Assets/Plugins/phonon.bundle")
shutil.copy("steamaudio_api/lib/Android/libphonon.so",   "src/project/SteamAudioUnity/Assets/Plugins/android")

print "Downloading steamaudio_tan_" + version + ".zip..."
url = "https://github.com/ValveSoftware/steam-audio/releases/download/v" + version + "/steamaudio_tan_" + version + ".zip"
download_file(url)

print "Extracting steamaudio_tan_" + version + ".zip..."
with zipfile.ZipFile(os.path.basename(url), "r") as zip:
    zip.extractall()

print "Copying files..."
shutil.copy("steamaudio_tan/bin/windows/x64/tanrt64.dll",      "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_tan/bin/windows/x64/GPUUtilities.dll", "src/project/SteamAudioUnity/Assets/Plugins/x86_64")

print "Downloading steamaudio_embree_" + version + ".zip..."
url = "https://github.com/ValveSoftware/steam-audio/releases/download/v" + version + "/steamaudio_embree_" + version + ".zip"
download_file(url)

print "Extracting steamaudio_embree_" + version + ".zip..."
with zipfile.ZipFile(os.path.basename(url), "r") as zip:
    zip.extractall()

print "Copying files..."
shutil.copy("steamaudio_embree/bin/windows/x64/embree.dll",         "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_embree/bin/windows/x64/tbb.dll",            "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_embree/bin/windows/x64/tbbmalloc.dll",      "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_embree/bin/linux/x64/libembree.so",         "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_embree/bin/linux/x64/libtbb.so.2",          "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_embree/bin/linux/x64/libtbbmalloc.so.2",    "src/project/SteamAudioUnity/Assets/Plugins/x86_64")
shutil.copy("steamaudio_embree/bin/osx/libembree.dylib",            "src/project/SteamAudioUnity/Assets/Plugins")
shutil.copy("steamaudio_embree/bin/osx/libtbb.dylib",               "src/project/SteamAudioUnity/Assets/Plugins")
shutil.copy("steamaudio_embree/bin/osx/libtbbmalloc.dylib",         "src/project/SteamAudioUnity/Assets/Plugins")

print "Downloading steamaudio_radeonrays_" + version + ".zip..."
url = "https://github.com/ValveSoftware/steam-audio/releases/download/v" + version + "/steamaudio_radeonrays_" + version + ".zip"
download_file(url)

print "Extracting steamaudio_radeonrays_" + version + ".zip..."
with zipfile.ZipFile(os.path.basename(url), "r") as zip:
    zip.extractall()

print "Copying files..."
shutil.copy("steamaudio_radeonrays/bin/windows/x64/RadeonRays.dll",     "src/project/SteamAudioUnity/Assets/Plugins/x86_64")

print "Cleaning up..."
shutil.rmtree("steamaudio_api")
shutil.rmtree("steamaudio_tan")
shutil.rmtree("steamaudio_embree")
shutil.rmtree("steamaudio_radeonrays")