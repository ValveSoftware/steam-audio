#
# Copyright 2017-2023 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import shutil
import urllib.request, urllib.error, urllib.parse
import zipfile

version = "4.6.0"

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
if not os.path.exists("lib/windows-x86"):
    os.makedirs("lib/windows-x86")
if not os.path.exists("lib/windows-x64"):
    os.makedirs("lib/windows-x64")
if not os.path.exists("lib/linux-x86"):
    os.makedirs("lib/linux-x86")
if not os.path.exists("lib/linux-x64"):
    os.makedirs("lib/linux-x64")
if not os.path.exists("lib/osx"):
    os.makedirs("lib/osx")
if not os.path.exists("lib/android-armv7"):
    os.makedirs("lib/android-armv7")
if not os.path.exists("lib/android-armv8"):
    os.makedirs("lib/android-armv8")
if not os.path.exists("lib/android-x86"):
    os.makedirs("lib/android-x86")

print("Copying files...")
shutil.copy("steamaudio/lib/windows-x86/phonon.dll", "lib/windows-x86")
shutil.copy("steamaudio/lib/windows-x64/phonon.dll", "lib/windows-x64")
shutil.copy("steamaudio/lib/linux-x86/libphonon.so", "lib/linux-x86")
shutil.copy("steamaudio/lib/linux-x64/libphonon.so", "lib/linux-x64")
try:
	shutil.rmtree("lib/osx/phonon.bundle")
except:
	pass
shutil.copytree("steamaudio/lib/osx/phonon.bundle", "lib/osx/phonon.bundle")
shutil.copy("steamaudio/lib/android-armv7/libphonon.so", "lib/android-armv7")
shutil.copy("steamaudio/lib/android-armv8/libphonon.so", "lib/android-armv8")
shutil.copy("steamaudio/lib/android-x86/libphonon.so", "lib/android-x86")

print("Cleaning up...")
shutil.rmtree("steamaudio")