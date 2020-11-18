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
# Deploys the Steam Audio Unity integration.
#
def deploy_integration_unity(configuration):
    print "Deploying: Steam Audio Unity Integration"

    deploy_path = os.path.abspath(os.path.join(os.getcwd(), "products"))

    os.makedirs(deploy_path + "\\steamaudio_unity")
    os.makedirs(deploy_path + "\\steamaudio_unity\\bin")
    os.makedirs(deploy_path + "\\steamaudio_unity\\bin\\unity")
    os.makedirs(deploy_path + "\\steamaudio_unity\\doc")

    copy(os.path.join(deploy_path, "..\\..\\bin\\SteamAudio.unitypackage"),
         deploy_path + "\\steamaudio_unity\\bin\\unity")
    copy(os.path.join(deploy_path, "..\\..\\doc\\phonon_unity.html"),
         deploy_path + "\\steamaudio_unity\\doc")

    zip_output = os.path.abspath(os.path.join(deploy_path, "steamaudio_unity.zip"))

    deploy_directory = os.getcwd()
    products_directory = deploy_directory + "\\products"
    os.chdir(products_directory)
    with zipfile.ZipFile(zip_output, "w", zipfile.ZIP_DEFLATED) as unity_zip:
        for directory, subdirectories, files in os.walk("steamaudio_unity"):
            for file in files:
                file_name = os.path.join(directory, file)
                print "Adding " + file_name + "..."
                unity_zip.write(file_name)
    os.chdir(deploy_directory)


#
# Package the TrueAudio Next libraries for use in all Steam Audio binaries.
#
def deploy_tan():
    print "Deploying: Steam Audio TrueAudio Next Support"

    deploy_path = os.path.abspath(os.path.join(os.getcwd(), "products"))
    deploy_path_tan = os.path.join(deploy_path, "steamaudio_tan")

    os.makedirs(deploy_path_tan)
    os.makedirs(os.path.join(deploy_path_tan, "bin"))
    os.makedirs(os.path.join(deploy_path_tan, "bin\\windows"))
    os.makedirs(os.path.join(deploy_path_tan, "bin\\windows\\x64"))
    os.makedirs(os.path.join(deploy_path_tan, "unity"))

    copy(os.path.join(os.getcwd(), "../../core/lib/windows-x64/tanrt64.dll"),
         os.path.join(deploy_path_tan, "bin/windows/x64"))
    copy(os.path.join(os.getcwd(), "../../core/lib/windows-x64/GPUUtilities.dll"),
         os.path.join(deploy_path_tan, "bin/windows/x64"))
    copy(os.path.join(os.getcwd(), "../bin/SteamAudio_TrueAudioNext.unitypackage"),
         os.path.join(deploy_path_tan, "unity"))

    zip_output = os.path.abspath(os.path.join(deploy_path, "steamaudio_tan.zip"))

    deploy_directory = os.getcwd()
    products_directory = deploy_directory + "\\products"
    os.chdir(products_directory)
    with zipfile.ZipFile(zip_output, "w", zipfile.ZIP_DEFLATED) as tan_zip:
        for directory, subdirectories, files in os.walk("steamaudio_tan"):
            for file in files:
                file_name = os.path.join(directory, file)
                print "Adding " + file_name + "..."
                tan_zip.write(file_name)
    os.chdir(deploy_directory)


#
# Package the Radeon Rays libraries for use in all Steam Audio binaries.
#
def deploy_radeonrays():
    print "Deploying: Steam Audio Radeon Rays Support"

    root_path = os.path.abspath(os.getcwd() + "/../..")

    deploy_path = os.path.abspath(os.getcwd() + "/products")
    deploy_path_radeonrays = deploy_path + "/steamaudio_radeonrays"

    os.makedirs(deploy_path_radeonrays + "/bin/windows/x64")
    os.makedirs(deploy_path_radeonrays + "/unity")

    copy(root_path + "/core/lib/windows-x64/RadeonRays.dll", deploy_path_radeonrays + "/bin/windows/x64")
    copy(root_path + "/core/lib/windows-x64/GPUUtilities.dll", deploy_path_radeonrays + "/bin/windows/x64")
    copy(root_path + "/unity/bin/SteamAudio_RadeonRays.unitypackage", deploy_path_radeonrays + "/unity")

    zip_output = os.path.abspath(os.path.join(deploy_path, "steamaudio_radeonrays.zip"))

    deploy_directory = os.getcwd()
    products_directory = deploy_directory + "/products"
    os.chdir(products_directory)
    with zipfile.ZipFile(zip_output, "w", zipfile.ZIP_DEFLATED) as radeonrays_zip:
        for directory, subdirectories, files in os.walk("steamaudio_radeonrays"):
            for file in files:
                file_name = os.path.join(directory, file)
                print "Adding " + file_name + "..."
                radeonrays_zip.write(file_name)
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
        deploy_integration_unity(configuration)
        deploy_tan()
        deploy_radeonrays()
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
