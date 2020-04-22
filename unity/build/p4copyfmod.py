#
# Copyright (c) Valve Corporation. All rights reserved.
#

import argparse
import os
import shutil
import subprocess

def generate_file_list(configuration):
	files = [
		["fmod/bin/windows-vs2015-x86-"+configuration+"/phonon_fmod.dll"	, "unity/src/project/SteamAudioUnity/Assets/Plugins/x86/phonon_fmod.dll"],
		["fmod/bin/windows-vs2015-x64-"+configuration+"/phonon_fmod.dll" 	, "unity/src/project/SteamAudioUnity/Assets/Plugins/x86_64/phonon_fmod.dll"],
		["fmod/bin/linux-x86-"+configuration+"/libphonon_fmod.so" 			, "unity/src/project/SteamAudioUnity/Assets/Plugins/x86/libphonon_fmod.so"],
		["fmod/bin/linux-x64-"+configuration+"/libphonon_fmod.so" 			, "unity/src/project/SteamAudioUnity/Assets/Plugins/x86_64/libphonon_fmod.so"],
		["fmod/bin/osx-"+configuration+"/phonon_fmod.bundle/..."			, "unity/src/project/SteamAudioUnity/Assets/Plugins/phonon_fmod.bundle/..."],
		["fmod/bin/android-armv7-"+configuration+"/libphonon_fmod.so" 		, "unity/src/project/SteamAudioUnity/Assets/Plugins/android/libs/armv7/libphonon_fmod.so"],
		["fmod/bin/android-arm64-"+configuration+"/libphonon_fmod.so" 		, "unity/src/project/SteamAudioUnity/Assets/Plugins/android/libs/arm64/libphonon_fmod.so"],
		["fmod/bin/android-x86-"+configuration+"/libphonon_fmod.so" 		, "unity/src/project/SteamAudioUnity/Assets/Plugins/android/libs/x86/libphonon_fmod.so"]
	]
	return files

def get_component_for_file(file):
	if "bin/windows-vs2015-x86" in file:
		return "win32"
	elif "bin/windows-vs2015-x64" in file:
		return "win64"
	elif "bin/linux-x86" in file:
		return "linux32"
	elif "bin/linux-x64" in file:
		return "linux64"
	elif "bin/osx" in file:
		return "osx"
	elif "bin/android" in file:
		return "android"
	else:
		print "WARNING: Unknown component for file " + file + "!"
		return ""

def component_includes(specifiedcomponent, testcomponent):
	return specifiedcomponent == testcomponent or specifiedcomponent == "all"

def copy(srcfile, destfile):
	srcfile = "../../" + srcfile
	destfile = "../../" + destfile
	p = subprocess.Popen("p4 copy " + srcfile + " " + destfile)
	stdout, stderr = p.communicate()

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--configuration", help="Build configuration.", choices=["debug", "development", "release"], type=str.lower)
parser.add_argument("-t", "--type", help="Which files to copy.", choices=["win32", "win64", "linux32", "linux64", "osx", "android", "all"], type=str.lower)
args = parser.parse_args()

if args.configuration is None:
	args.configuration = "release"

if args.type is None:
	args.type = "all"

files = generate_file_list(args.configuration)
for file in files:
	component = get_component_for_file(file[0])
	if component_includes(args.type, component):
		copy(file[0], file[1])
