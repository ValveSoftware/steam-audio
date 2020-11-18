#
# Copyright (c) Valve Corporation. All rights reserved.
#

import argparse
import os
import subprocess
import sys
import shutil


###############################################################################
# HOST SYSTEM FUNCTIONS
###############################################################################

#
# Detects the host operating system.
#
def detect_host_system():

    if sys.platform == 'win32':
        return "Windows"
    elif sys.platform == 'darwin':
        return "Mac OS X"
    elif sys.platform in ['linux', 'linux2', 'linux3']:
        return "Linux"
    else:
        return "Unknown"


#
# Sets tool paths based on the host system.
#
def get_tool_paths(host_system):

    path_cmake      = ""
    path_gcc        = ""
    path_make       = ""
    path_sysroot    = ""
    path_android    = ""
    path_ant        = ""

    # On Windows, and Linux, CMake is assumed to be in the search path. On
    # Mac OS X, we specify the path explicitly.
    if host_system == "Mac OS X":
        path_cmake = "/Applications/CMake.app/Contents/bin/"

    # On Windows, configure the Android SDK and NDK toolchains.
    if host_system == "Windows":
        path_gcc        = "C:/Android/sdk/standalone-toolchains/android-armv7/bin/"
        path_make       = "C:/Android/sdk/ndk-bundle/prebuilt/windows-x86_64/bin/"
        path_sysroot    = "C:/Android/sdk/standalone-toolchains/android-armv7/sysroot/"
        path_android    = "C:/Android/sdk/tools/"
        path_ant        = "C:/Android/ant/bin/"

    return argparse.Namespace(
        cmake   = path_cmake,
        gcc     = path_gcc,
        make    = path_make,
        sysroot = path_sysroot,
        android = path_android,
        ant     = path_ant
    )


#
# Parses the command line.
#
def parse_command_line(host_system):

    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--platform', help = "Target operating system.", choices = ['windows', 'osx', 'linux', 'android'], type = str.lower)
    parser.add_argument('-t', '--toolchain', help = argparse.SUPPRESS, choices = ['vs2013', 'vs2015', 'vs2017'], type = str.lower)
    parser.add_argument('-a', '--architecture', help = "CPU architecture.", choices = ['x86', 'x64', 'armv7', 'arm64'], type = str.lower)
    parser.add_argument('-c', '--configuration', help = "Build configuration.", choices = ['debug', 'development', 'release'], type = str.lower)
    parser.add_argument('-i', '--integration', help = "Integration.", choices = ['unity'], type = str.lower)
    parser.add_argument('--ci', help = "Build in CI mode.", dest = 'ci', action = 'store_true')
    parser.add_argument('--versionstamp', help = "Update version stamps in the source code based on CMake project configuration.", action = 'store_true')
    parser.set_defaults(ci = False)
    args = parser.parse_args()

    # If the platform was not specified, set it to the host platform.
    if args.platform is None:
        if host_system == "Windows":
            args.platform = 'windows'
        elif host_system == "Mac OS X":
            args.platform = 'osx'
        elif host_system == "Linux":
            args.platform = 'linux'
        else:
            args.platform = ''

    # If the toolchain was not specified, and the platform is Windows, use
    # Visual Studio 2015.
    if args.toolchain is None:
        if args.platform == 'windows':
            args.toolchain = 'vs2015'
        else:
            args.toolchain = ''

    # If the architecture was not specified, use x64 on Windows and Linux,
    # and armv7 on Android.
    if args.architecture is None:
        if args.platform in ['windows', 'linux']:
            args.architecture = 'x64'
        elif args.platform == 'android':
            args.architecture = 'armv7'
        else:
            args.architecture = ''

    # If the configuration was not specified, use Release, except with Windows
    # and Visual Studio 2015, in which case use Development.
    if args.configuration is None:
        args.configuration = 'release'

    if args.integration is None:
        args.integration = ''

    if args.ci is None:
        args.ci = False

    return args


###############################################################################
# HELPER FUNCTIONS
###############################################################################

#
# Builds Unity integration.
#
def build_unity(args):

    if args.platform != 'windows':
        print "ERROR: Unity integration must be built on Windows!"
        return

    unity = "C:\\Program Files\\Unity\\Editor\\Unity.exe"

    path_to_binaries = os.path.abspath(os.path.join(os.getcwd(), "..\\bin"))
    path_to_unity_base = os.path.abspath(os.path.join(os.getcwd(), "..\\src\\project\\SteamAudioUnity"))

    bin_dir = os.path.abspath(os.path.join(os.getcwd(), "..", "bin"))

    windows_x86_native_bin = os.path.join(bin_dir, "windows-vs2015-x86-" + args.configuration, "audioplugin_phonon.dll")
    windows_x64_native_bin = os.path.join(bin_dir, "windows-vs2015-x64-" + args.configuration, "audioplugin_phonon.dll")
    linux_x86_native_bin = os.path.join(bin_dir, "linux-x86-" + args.configuration, "libaudioplugin_phonon.so")
    linux_x64_native_bin = os.path.join(bin_dir, "linux-x64-" + args.configuration, "libaudioplugin_phonon.so")
    osx_native_bin = os.path.join(bin_dir, "osx-" + args.configuration, "audioplugin_phonon.bundle")
    android_native_bin = os.path.join(bin_dir, "android-armv7-" + args.configuration, "libaudioplugin_phonon.so")

    unity_project_dir = os.path.abspath(os.path.join(os.getcwd(), "..", "src", "project", "steamaudiounity"))
    unity_plugins_dir = os.path.join(unity_project_dir, "assets", "plugins")

    windows_x86_plugin_dir = os.path.join(unity_plugins_dir, "x86")
    windows_x64_plugin_dir = os.path.join(unity_plugins_dir, "x86_64")
    linux_x86_plugin_dir = os.path.join(unity_plugins_dir, "x86")
    linux_x64_plugin_dir = os.path.join(unity_plugins_dir, "x86_64")
    osx_plugin_dir = unity_plugins_dir
    android_plugin_dir = os.path.join(unity_plugins_dir, "android")

    windows_x86_audioplugin = os.path.join(windows_x86_plugin_dir, "audioplugin_phonon.dll")
    windows_x64_audioplugin = os.path.join(windows_x64_plugin_dir, "audioplugin_phonon.dll")
    linux_x86_audioplugin = os.path.join(linux_x86_plugin_dir, "libaudioplugin_phonon.so")
    linux_x64_audioplugin = os.path.join(linux_x64_plugin_dir, "libaudioplugin_phonon.so")
    osx_audioplugin = os.path.join(osx_plugin_dir, "audioplugin_phonon.bundle")
    android_audioplugin = os.path.join(android_plugin_dir, "libaudioplugin_phonon.so")

    if not os.path.exists(windows_x86_plugin_dir):
        os.makedirs(windows_x86_plugin_dir)
    if not os.path.exists(windows_x64_plugin_dir):
        os.makedirs(windows_x64_plugin_dir)
    if not os.path.exists(android_plugin_dir):
        os.makedirs(android_plugin_dir)

    if os.path.exists(osx_audioplugin):
        shutil.rmtree(osx_audioplugin, ignore_errors=True)

    shutil.copyfile(windows_x64_native_bin, windows_x64_audioplugin)
    if args.ci:
        shutil.copyfile(windows_x86_native_bin, windows_x86_audioplugin)
        shutil.copyfile(linux_x86_native_bin, linux_x86_audioplugin)
        shutil.copyfile(linux_x64_native_bin, linux_x64_audioplugin)
        shutil.copytree(osx_native_bin, osx_audioplugin)
        shutil.copyfile(android_native_bin, android_audioplugin)

    # Run build commands
    print "Building Unity plugin..."
    p = subprocess.Popen(unity + " -batchmode -quit -projectPath " + unity_project_dir + " -executeMethod SteamAudio.Build.BuildSteamAudio " + args.configuration, cwd=os.getcwd())
    stdout, stderr = p.communicate()

    print "Building Unity + FMOD Studio plugin..."
    p = subprocess.Popen(unity + " -batchmode -quit -projectPath " + unity_project_dir + " -executeMethod SteamAudio.Build.BuildSteamAudioFMODStudio " + args.configuration, cwd=os.getcwd())
    stdout, stderr = p.communicate()

    print "Building Unity + TrueAudio Next support..."
    p = subprocess.Popen(unity + " -batchmode -quit -projectPath " + unity_project_dir + " -executeMethod SteamAudio.Build.BuildSteamAudioTrueAudioNext " + args.configuration, cwd=os.getcwd())
    stdout, stderr = p.communicate()

    print "Building Unity + Radeon Rays support..."
    p = subprocess.Popen(unity + " -batchmode -quit -projectPath " + unity_project_dir + " -executeMethod SteamAudio.Build.BuildSteamAudioRadeonRays " + args.configuration, cwd=os.getcwd())
    stdout, stderr = p.communicate()

    shutil.copyfile(os.path.join(unity_project_dir, "SteamAudio.unitypackage"), os.path.join(bin_dir, "SteamAudio.unitypackage"))
    shutil.copyfile(os.path.join(unity_project_dir, "SteamAudio_FMODStudio.unitypackage"), os.path.join(bin_dir, "SteamAudio_FMODStudio.unitypackage"))
    shutil.copyfile(os.path.join(unity_project_dir, "SteamAudio_TrueAudioNext.unitypackage"), os.path.join(bin_dir, "SteamAudio_TrueAudioNext.unitypackage"))
    shutil.copyfile(os.path.join(unity_project_dir, "SteamAudio_RadeonRays.unitypackage"), os.path.join(bin_dir, "SteamAudio_RadeonRays.unitypackage"))


#
# Returns the subdirectory in which to create build files.
#
def get_build_dir_name(args):

    if args.platform == 'windows':
        return "-".join([args.platform, args.toolchain, args.architecture])
    elif args.platform == 'osx':
        return args.platform
    elif args.platform in ['linux', 'android']:
        return "-".join([args.platform, args.architecture, args.configuration])


#
# Returns the subdirectory in which to create output files.
#
def get_bin_dir_name(args):

    if args.platform == 'windows':
        return "-".join([args.platform, args.toolchain, args.architecture])
    elif args.platform == 'osx':
        return "-".join([args.platform])
    elif args.platform in ['linux', 'android']:
        return "-".join([args.platform, args.architecture, args.configuration])


#
# Returns the generator name to pass to CMake, based on the platform.
#
def get_cmake_generator_name(args):
    if args.platform == 'windows':
        suffix = '';
        if args.architecture == 'x64':
            suffix = ' Win64';

        generator = '';            
        if args.toolchain == 'vs2013':
            generator = 'Visual Studio 12 2013'
        elif args.toolchain == 'vs2015':
            generator = 'Visual Studio 14 2015'
        elif args.toolchain == 'vs2017':
            generator = 'Visual Studio 15 2017'

        return generator + suffix        
    elif args.platform == 'osx':
        return 'Xcode'
    elif args.platform in ['linux', 'android']:
        return 'Unix Makefiles'


#
# Returns the configuration name to pass to CMake.
#
def get_cmake_config_name(args):

    # For the time being, we build development builds as Release.
    if args.configuration == 'debug':
        return "Debug"
    elif args.configuration == 'development':
        return "Development"
    else:
        return "Release"


#
# Runs CMake.
#
def run_cmake(paths, cmake_args):

    subprocess.call([paths.cmake + "cmake"] + cmake_args)


###############################################################################
# BUILD STAGES
###############################################################################

#
# Generates a build system using CMake.
#
def generate_build_system(paths, args):

    generator_name = get_cmake_generator_name(args)
    build_config = get_cmake_config_name(args)
    build_dir = get_build_dir_name(args)
    bin_dir = get_bin_dir_name(args)

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    original_dir = os.getcwd()
    os.chdir(build_dir)

    cmake_args = ["-G", generator_name, "../.."]

    # Output files go in /bin.
    cmake_args = cmake_args + ["-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=" + original_dir + "/../bin/" + bin_dir]
    cmake_args = cmake_args + ["-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" + original_dir + "/../bin/" + bin_dir]
    cmake_args = cmake_args + ["-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=" + original_dir + "/../bin/" + bin_dir]

    # For Linux or Android, the configuration must be set when generating the
    # Makefiles.
    if args.platform == 'linux' or args.platform == 'android':
        cmake_args = cmake_args + ["-DCMAKE_BUILD_TYPE=" + build_config]

    # For Linux, the architecture must be set when generating the Makefiles.
    # The default is x64.
    if args.platform == 'linux' and args.architecture == 'x86':
        cmake_args = cmake_args + ["-DCMAKE_CXX_FLAGS=-m32", "-DCMAKE_SHARED_LINKER_FLAGS=-m32"]

    # For Android, a cross-compilation toolchain must be specified. This
    # toolchain assumes ARMv7 as the architecture. The toolchain can be created
    # using the Android NDK utility "make-standalone-toolchain".
    if args.platform == 'android':
        if args.architecture == "armv7":
            cmake_args = cmake_args + ["-DCMAKE_TOOLCHAIN_FILE=build/toolchain_android_armv7.cmake"]
        elif args.architecture == "arm64":
            cmake_args = cmake_args + ["-DCMAKE_TOOLCHAIN_FILE=build/toolchain_android_armv8.cmake"]
        elif args.architecture == "x86":
            cmake_args = cmake_args + ["-DCMAKE_TOOLCHAIN_FILE=build/toolchain_android_x86.cmake"]
        elif args.architecture == "x64":
            cmake_args = cmake_args + ["-DCMAKE_TOOLCHAIN_FILE=build/toolchain_android_x64.cmake"]

    if args.versionstamp:
        cmake_args = cmake_args + ["-DUPDATE_VERSION_STAMPS=TRUE"]
    else:
        cmake_args = cmake_args + ["-DUPDATE_VERSION_STAMPS=FALSE"]

    run_cmake(paths, cmake_args)
    os.chdir(original_dir)


#
# Implements the 'build' action. Calls CMake, which in turn calls the
# appropriate toolchain to build.
#
def build(paths, args):

    build_dir = get_build_dir_name(args)
    build_config = get_cmake_config_name(args)

    run_cmake(paths, ["--build", build_dir, "--config", build_config])


###############################################################################
# MAIN SCRIPT
###############################################################################

host_system = detect_host_system()
print "Host system is " + host_system + "."

paths = get_tool_paths(host_system)
print "Using build tools from the following locations:"
print "     CMake               : " + paths.cmake
print "     GCC                 : " + paths.gcc
print "     GCC System Root     : " + paths.sysroot
print "     Make                : " + paths.make
print "     Android SDK Manager : " + paths.android
print "     Ant                 : " + paths.ant

args = parse_command_line(host_system)
print "Build system settings:"
print "    Platform      : " + args.platform
print "    Toolchain     : " + args.toolchain
print "    Architecture  : " + args.architecture
print "    Configuration : " + args.configuration
print "    Integration   : " + args.integration

if args.integration == "unity":
    build_unity(args)
else:
    generate_build_system(paths, args)
    if args.versionstamp is not True:
        build(paths, args)
