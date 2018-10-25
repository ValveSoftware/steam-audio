#
# Copyright (c) Valve Corporation. All rights reserved.
#

import argparse
import os
import shutil
import subprocess
import distutils.dir_util
import distutils.file_util

def copy(source, destination):
	source = "../../" + source
	destination = "../../" + destination
	if os.path.isdir(source):
		distutils.dir_util.copy_tree(source, destination, 0)
	else:
		distutils.file_util.copy_file(source, destination, 0)
	print source + " -> " + destination

def component_includes(specifiedcomponent, testcomponent):
	return specifiedcomponent == testcomponent or specifiedcomponent == "all"

def update_core(component, configuration):
	if component_includes(component, "win32"):
		copy("fmod/bin/windows-vs2015-x86-"+configuration+"/phonon_fmod.dll", "unity/src/project/SteamAudioUnity/Assets/Plugins/x86/phonon_fmod.dll")
		copy("fmod/bin/windows-vs2015-x86-"+configuration+"/phonon_fmod.pdb", "unity/src/project/SteamAudioUnity/Assets/Plugins/x86/phonon_fmod.pdb")
	if component_includes(component, "win64"):
		copy("fmod/bin/windows-vs2015-x64-"+configuration+"/phonon_fmod.dll", "unity/src/project/SteamAudioUnity/Assets/Plugins/x86_64/phonon_fmod.dll")
		copy("fmod/bin/windows-vs2015-x64-"+configuration+"/phonon_fmod.pdb", "unity/src/project/SteamAudioUnity/Assets/Plugins/x86_64/phonon_fmod.pdb")
	if component_includes(component, "linux32"):
		copy("fmod/bin/linux-x86-"+configuration+"/libphonon_fmod.so", "unity/src/project/SteamAudioUnity/Assets/Plugins/x86/libphonon_fmod.so")
	if component_includes(component, "linux64"):
		copy("fmod/bin/linux-x64-"+configuration+"/libphonon_fmod.so", "unity/src/project/SteamAudioUnity/Assets/Plugins/x86_64/libphonon_fmod.so")
	if component_includes(component, "osx"):
		copy("fmod/bin/osx-"+configuration+"/phonon_fmod.bundle", "unity/src/project/SteamAudioUnity/Assets/Plugins/phonon_fmod.bundle")
	if component_includes(component, "android"):
		copy("fmod/bin/android-armv7-"+configuration+"/libphonon_fmod.so", "unity/src/project/SteamAudioUnity/Assets/Plugins/android/libphonon_fmod.so")

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--configuration", help="Build configuration.", choices=["debug", "development", "release"], type=str.lower)
parser.add_argument("-t", "--type", help="Which files to copy.", choices=["header", "win32", "win64", "linux32", "linux64", "osx", "android", "all"], type=str.lower)
args = parser.parse_args()

if args.configuration is None:
	args.configuration = "release"

if args.type is None:
	args.type = "all"

update_core(args.type, args.configuration)