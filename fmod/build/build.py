# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

import argparse
import os
import subprocess
import sys
import shutil

# Detects the host operating system.
def detect_host_system():
    if sys.platform == 'win32':
        return 'windows'
    elif sys.platform == 'darwin':
        return 'osx'
    elif sys.platform in ['linux', 'linux2', 'linux3']:
        return 'linux'
    else:
        return ''

# Parses the command line.
def parse_command_line(host_system):
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--platform', help = "Target operating system.", choices = ['windows', 'osx', 'linux', 'android'], type = str.lower, default = host_system)
    parser.add_argument('-t', '--toolchain', help = "Compiler toolchain. (Windows only)", choices = ['vs2013', 'vs2015', 'vs2017', 'vs2019'], type = str.lower, default = 'vs2015')
    parser.add_argument('-a', '--architecture', help = "CPU architecture.", choices = ['x86', 'x64', 'armv7', 'arm64'], type = str.lower, default = 'x64')
    parser.add_argument('-c', '--configuration', help = "Build configuration.", choices = ['debug', 'release'], type = str.lower, default = 'release')
    parser.add_argument('-o', '--operation', help = "CMake operation.", choices = ['generate', 'build', 'install', 'package', 'default'], type = str.lower, default = 'default')
    args = parser.parse_args()
    return args

# Returns the subdirectory in which to create build files.
def build_subdir(args):
    if args.platform == 'windows':
        return "-".join([args.platform, args.toolchain, args.architecture])
    elif args.platform == 'osx':
        return args.platform
    elif args.platform in ['linux', 'android']:
        return "-".join([args.platform, args.architecture, args.configuration])

# Returns the subdirectory in which to create output files.
def bin_subdir(args):
    if args.platform in ['windows', 'linux', 'android']:
        return "-".join([args.platform, args.architecture])
    elif args.platform == 'osx':
        return "-".join([args.platform])

# Returns the root directory of the repository.
def root_dir():
    return os.path.normpath(os.getcwd() + '/../..')

# Returns the generator name to pass to CMake, based on the platform.
def generator_name(args):
    if args.platform == 'windows':
        suffix = '';
        if args.architecture == 'x64' and args.toolchain != 'vs2019':
            suffix = ' Win64';
        generator = '';            
        if args.toolchain == 'vs2013':
            generator = 'Visual Studio 12 2013'
        elif args.toolchain == 'vs2015':
            generator = 'Visual Studio 14 2015'
        elif args.toolchain == 'vs2017':
            generator = 'Visual Studio 15 2017'
        elif args.toolchain == 'vs2019':
            generator = 'Visual Studio 16 2019'
        return generator + suffix        
    elif args.platform == 'osx':
        return 'Xcode'
    elif args.platform in ['linux', 'android']:
        return 'Unix Makefiles'

# Returns the configuration name to pass to CMake.
def config_name(args):
    if args.configuration == 'debug':
        return "Debug"
    else:
        return "Release"

# Runs CMake (or CTest, or CPack). 
def run_cmake(program_name, args):
    env = os.environ.copy()
    if os.getenv('STEAMAUDIO_OVERRIDE_PYTHONPATH') is not None:
        env['PYTHONPATH'] = ''

    subprocess.check_call([program_name] + args, env=env)

# Runs the "generate" step.
def cmake_generate(args):
    cmake_args = ['-G', generator_name(args)]

    # If using Visual Studio 2019 on Windows, specify architecture as a parameter.
    if args.platform == 'windows' and args.toolchain == 'vs2019':
        if args.architecture == 'x86':
            cmake_args += ['-A', 'Win32']
        elif args.architecture == 'x64':
            cmake_args += ['-A', 'x64']

    # For Android, specify the toolchain file and point to the NDK installation.
    if args.platform == 'android':
        if args.architecture == 'armv7':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_armv7.cmake']
        elif args.architecture == 'arm64':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_armv8.cmake']
        elif args.architecture == 'x86':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_x86.cmake']
        elif args.architecture == 'x64':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_x64.cmake']

        cmake_args += ['-DCMAKE_ANDROID_NDK=C:/Android/sdk/ndk-bundle']
        cmake_args += ['-DCMAKE_MAKE_PROGRAM=C:/Android/sdk/ndk-bundle/prebuilt/windows-x86_64/bin/make.exe']

    # On Linux and Android, specify the build configuration at generate-time.
    if args.platform in ['linux', 'android']:
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + config_name(args)]

    # Install files to bin/.
    cmake_args += ['-DCMAKE_INSTALL_PREFIX=' + root_dir() + '/bin']

    # On Linux x86, specify -m32 explicitly.
    if args.platform == 'linux' and args.architecture == 'x86':
        cmake_args += ['-DCMAKE_CXX_FLAGS=-m32']
        cmake_args += ['-DCMAKE_SHARED_LINKER_FLAGS=-m32']

    # On Android, explitly point to dependencies.
    if args.platform == 'android':
        cmake_args += ['-DFMOD_INCLUDE_DIR=' + root_dir() + '/include/fmod']
        cmake_args += ['-DSteamAudio_INCLUDE_DIR=' + root_dir() + '/include/phonon']

    # On Windows x64, build documentation.
    if args.platform == 'windows' and args.architecture == 'x64':
        cmake_args += ['-DSTEAMAUDIOFMOD_BUILD_DOCS=TRUE']

    cmake_args += ['../..']

    run_cmake('cmake', cmake_args)

# Runs the "build" step.
def cmake_build(args):
    cmake_args = ['--build', '.']
    
    if args.platform in ['windows', 'osx']:
        cmake_args += ['--config', config_name(args)]
    
    run_cmake('cmake', cmake_args)

# Runs the "install" step.
def cmake_install(args):
    cmake_args = ['--install', '.']

    if args.platform in ['windows', 'osx']:
        cmake_args += ['--config', config_name(args)]

    run_cmake('cmake', cmake_args)

# Runs the "package" step.
def cmake_package(args):
    cmake_args = ['-G', 'ZIP']

    if args.platform in ['windows', 'osx']:
        cmake_args += ['-C', config_name(args)]

    run_cmake('cpack', cmake_args)

# Main script.

host_system = detect_host_system()
args = parse_command_line(host_system)

try:
    os.makedirs(build_subdir(args))
except Exception as e:
    pass

olddir = os.getcwd()
os.chdir(build_subdir(args))

if args.operation == 'generate':
    cmake_generate(args)
elif args.operation == 'build':
    cmake_build(args)
elif args.operation == 'install':
    cmake_install(args)
elif args.operation == 'package':
    cmake_package(args)
elif args.operation == 'default':
    cmake_generate(args)
    cmake_build(args)

os.chdir(olddir)
