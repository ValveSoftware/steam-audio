#
# Copyright (c) Valve Corporation. All rights reserved.
#

import argparse
import os
import shutil
import subprocess

def copy(source, destination):
	source = "../../" + source
	destination = "../../" + destination
	if os.path.isdir(source):
		if os.path.exists(destination):
			shutil.rmtree(destination)
		shutil.copytree(source, destination)
	else:
		shutil.copy(source, destination)
	print source + " -> " + destination

def component_includes(specifiedcomponent, testcomponent):
	return specifiedcomponent == testcomponent or specifiedcomponent == "all"

def update_core(component, configuration):
	if component_includes(component, "header"):
		copy("core/src/core/api/phonon.h", "fmod/include/phonon/phonon.h")
		copy("core/src/core/api/phonon_version.h", "fmod/include/phonon/phonon_version.h")
	if component_includes(component, "win32"):
		copy("core/bin/windows-vs2015-x86-"+configuration+"/phonon.dll", "fmod/lib/windows-x86/phonon.dll")
		copy("core/bin/windows-vs2015-x86-"+configuration+"/phonon.pdb", "fmod/lib/windows-x86/phonon.pdb")
	if component_includes(component, "win64"):
		copy("core/bin/windows-vs2015-x64-"+configuration+"/phonon.dll", "fmod/lib/windows-x64/phonon.dll")
		copy("core/bin/windows-vs2015-x64-"+configuration+"/phonon.pdb", "fmod/lib/windows-x64/phonon.pdb")
	if component_includes(component, "linux32"):
		copy("core/bin/linux-x86-"+configuration+"/libphonon.so", "fmod/lib/linux-x86/libphonon.so")
	if component_includes(component, "linux64"):
		copy("core/bin/linux-x64-"+configuration+"/libphonon.so", "fmod/lib/linux-x64/libphonon.so")
	if component_includes(component, "osx"):
		copy("core/bin/osx-"+configuration+"/libphonon.dylib", "fmod/lib/osx/libphonon.dylib")
		copy("core/bin/osx-"+configuration+"/phonon_bundle.bundle", "fmod/lib/osx/phonon.bundle")
	if component_includes(component, "android"):
		copy("core/bin/android-armv7-"+configuration+"/libphonon.so", "fmod/lib/android-armv7/libphonon.so")

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--configuration", help="Build configuration.", choices=["debug", "development", "release"], type=str.lower)
parser.add_argument("-t", "--type", help="Which files to copy.", choices=["header", "win32", "win64", "linux32", "linux64", "osx", "android", "all"], type=str.lower)
args = parser.parse_args()

if args.configuration is None:
	args.configuration = "release"

if args.type is None:
	args.type = "all"

update_core(args.type, args.configuration)