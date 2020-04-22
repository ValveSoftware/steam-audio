#
# Copyright (c) Valve Corporation. All rights reserved.
#

import argparse
import os
import sys
import shutil
import zipfile


###############################################################################
# HELPER FUNCTIONS
###############################################################################


#
# Helper function to copy files from src to dst.
#
def copy(src, dst):
    if os.path.isdir(dst):
        dst = os.path.join(dst, os.path.basename(src))
    shutil.copyfile(src, dst)


#
# Deploys the Steam Audio FMOD Studio integration.
#
def deploy_integration_fmod(configuration):
    print "Deploying: Steam Audio FMOD Studio Integration"

    deploy_path = os.path.abspath(os.path.join(os.getcwd(), "products"))

    os.makedirs(deploy_path + "\\steamaudio_fmod")
    os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod")
    os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\windows")
    os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\windows\\x86")
    os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\windows\\x64")
    #os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\linux")
    #os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\linux\\x86")
    #os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\linux\\x64")
    os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\osx")
    #os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\android")
    #os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\android\\armv7")
    #os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\android\\arm64")
    #os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\fmod\\android\\x86")
    os.makedirs(deploy_path + "\\steamaudio_fmod\\bin\\unity")
    os.makedirs(deploy_path + "\\steamaudio_fmod\\doc")

    # Copy FMOD Studio plugin - phonon.dll.
    copy(os.path.join(deploy_path, "..\\..\\lib\\windows-x86\\phonon.dll"),
         os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\windows\\x86"))
    copy(os.path.join(deploy_path, "..\\..\\lib\\windows-x64\\phonon.dll"),
         os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\windows\\x64"))
    # copy(os.path.join(deploy_path, "..\\..\\lib\\linux-x86\\libphonon.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\linux\\x86"))
    # copy(os.path.join(deploy_path, "..\\..\\lib\\linux-x64\\libphonon.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\linux\\x64"))
    # copy(os.path.join(deploy_path, "..\\..\\lib\\android-armv7\\libphonon.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\android\\armv7"))
    # copy(os.path.join(deploy_path, "..\\..\\lib\\android-arm64\\libphonon.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\android\\arm64"))
    # copy(os.path.join(deploy_path, "..\\..\\lib\\android-x86\\libphonon.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\android\\x86"))
    shutil.copytree(os.path.join(deploy_path, "..\\..\\lib\\osx\\phonon.bundle"),
           os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\osx\\phonon.bundle"))

    # Copy FMOD Studio plugin - phonon_fmod.dll.
    copy(os.path.join(deploy_path, "..\\..\\bin\\windows-vs2015-x86-" + configuration + "\\phonon_fmod.dll"),
         os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\windows\\x86"))
    copy(os.path.join(deploy_path, "..\\..\\bin\\windows-vs2015-x64-" + configuration + "\\phonon_fmod.dll"),
         os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\windows\\x64"))
    # copy(os.path.join(deploy_path, "..\\..\\bin\\linux-x86-" + configuration + "\\libphonon_fmod.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\linux\\x86"))
    # copy(os.path.join(deploy_path, "..\\..\\bin\\linux-x64-" + configuration + "\\libphonon_fmod.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\linux\\x64"))
    # copy(os.path.join(deploy_path, "..\\..\\bin\\android-armv7-" + configuration + "\\libphonon_fmod.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\android\\armv7"))
    # copy(os.path.join(deploy_path, "..\\..\\bin\\android-arm64-" + configuration + "\\libphonon_fmod.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\android\\arm64"))
    # copy(os.path.join(deploy_path, "..\\..\\bin\\android-x86-" + configuration + "\\libphonon_fmod.so"),
    #      os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod\\android\\x86"))
    copy(deploy_path + "/../../bin/osx-" + configuration + "/libphonon_fmod.dylib",
         deploy_path + "/steamaudio_fmod/bin/fmod/osx/libphonon_fmod.dylib")
    
    copy(os.path.join(deploy_path, "..\\..\\src\\phonon_fmod.plugin.js"),
         os.path.join(deploy_path, ".\\steamaudio_fmod\\bin\\fmod"))

    # Copy Unity + FMOD Studio plugin.
    copy(os.path.join(deploy_path, "..\\..\\..\\unity\\bin\\SteamAudio_FMODStudio.unitypackage"),
         deploy_path + "\\steamaudio_fmod\\bin\\unity")

    # Copy documentation.
    copy(os.path.join(deploy_path, "..\\..\\doc\\phonon_fmod.html"),
         os.path.join(deploy_path, ".\\steamaudio_fmod\\doc"))

    zip_output = os.path.abspath(os.path.join(deploy_path, "steamaudio_fmod.zip"))

    deploy_directory = os.getcwd()
    products_directory = deploy_directory + "\\products"
    os.chdir(products_directory)
    with zipfile.ZipFile(zip_output, "w", zipfile.ZIP_DEFLATED) as fmod_zip:
        for directory, subdirectories, files in os.walk("steamaudio_fmod"):
            for file in files:
                file_name = os.path.join(directory, file)
                print "Adding " + file_name + "..."
                fmod_zip.write(file_name)
    os.chdir(deploy_directory)


#
# Main deployment routine: deploys all products.
#
def deploy(configuration):
    print "Deploying: Steam Audio using " + configuration + " configuration"

    deploy_path = os.path.abspath(os.path.join(os.getcwd(), "products"))

    if os.path.exists(deploy_path):
        shutil.rmtree(deploy_path)

    os.makedirs(deploy_path)

    try:
        deploy_integration_fmod(configuration)
    except Exception as e:
        print "Exception encountered during deployment."
        print e
        sys.exit(1)


###############################################################################
# MAIN SCRIPT
###############################################################################


parser = argparse.ArgumentParser()
parser.add_argument("-c", "--configuration", help="Build configuration.", choices=["debug", "development", "release"],
                    type=str.lower)
args = parser.parse_args()

deploy(args.configuration)
