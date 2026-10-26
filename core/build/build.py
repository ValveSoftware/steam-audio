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

import argparse
import os
import platform
import re
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

# Detects the host CPU architecture ('x64' or 'arm64').
def detect_host_architecture():
    machine = platform.machine().lower()
    if sys.platform == 'win32':
        # An x64 Python running under emulation on Windows on ARM reports AMD64, so check the native architecture.
        machine = os.environ.get('PROCESSOR_ARCHITEW6432', os.environ.get('PROCESSOR_ARCHITECTURE', machine)).lower()
    if machine in ['aarch64', 'arm64', 'armv8', 'armv8l']:
        return 'arm64'
    else:
        return 'x64'

# Parses the command line.
def parse_command_line(host_system):
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--platform', help = "Target operating system.", choices = ['windows', 'osx', 'linux', 'android', 'ios', 'wasm'], type = str.lower, default = host_system)
    parser.add_argument('-t', '--toolchain', help = "Compiler toolchain. (Windows only)", choices = ['vs2013', 'vs2015', 'vs2017', 'vs2019', 'vs2022', 'vs2026'], type = str.lower, default = 'vs2019')
    parser.add_argument('-a', '--architecture', help = "CPU architecture.", choices = ['x86', 'x64', 'armv7', 'arm64', 'arm64ec', 'arm64x'], type = str.lower, default = 'x64')
    parser.add_argument('-c', '--configuration', help = "Build configuration.", choices = ['debug', 'release'], type = str.lower, default = 'release')
    parser.add_argument('-o', '--operation', help = "CMake operation.", choices = ['generate', 'build', 'test', 'install', 'package', 'default', 'ci_build', 'ci_package'], type = str.lower, default = 'default')
    parser.add_argument('--minimal', action='store_true', help='Use build options that minimize dependencies.')
    parser.add_argument('--unity', action='store_true', help='Configure a unity build (for improved build performance on CI systems).')
    parser.add_argument('--parallel', type=int, default=0, help='Number of threads to use when building. 0 = no threading.')
    args = parser.parse_args()
    return args

# Returns the subdirectory in which to create build files.
def build_subdir(args):
    if args.platform == 'windows':
        if args.arm64x_pass == 'ARM64':
            return "-".join([args.platform, args.toolchain, 'arm64x', 'arm64'])
        elif args.arm64x_pass == 'ARM64EC':
            return "-".join([args.platform, args.toolchain, 'arm64x'])
        return "-".join([args.platform, args.toolchain, args.architecture])
    elif args.platform in ['osx', 'ios']:
        return args.platform
    elif args.platform in ['linux', 'android']:
        return "-".join([args.platform, args.architecture, args.configuration])
    elif args.platform in ['wasm']:
        return "-".join([args.platform, args.configuration])

# Returns the root directory of the repository.
def root_dir():
    return os.path.normpath(os.getcwd() + '/../..')

# Returns the generator name to pass to CMake, based on the platform.
def generator_name(args):
    if args.platform == 'windows':
        suffix = ''
        generator = ''
        if args.toolchain == 'vs2013':
            generator = 'Visual Studio 12 2013'
        elif args.toolchain == 'vs2015':
            generator = 'Visual Studio 14 2015'
        elif args.toolchain == 'vs2017':
            generator = 'Visual Studio 15 2017'
        elif args.toolchain == 'vs2019':
            generator = 'Visual Studio 16 2019'
        elif args.toolchain == 'vs2022':
            generator = 'Visual Studio 17 2022'
        elif args.toolchain == 'vs2026':
            generator = 'Visual Studio 18 2026'
        return generator + suffix
    elif args.platform in ['osx', 'ios']:
        return 'Xcode'
    elif args.platform in ['linux', 'android', 'wasm']:
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
    if os.getenv('STEAMAUDIO_OVERRIDE_SDKROOT') is not None:
        env['SDKROOT'] = ''

    subprocess.check_call([program_name] + args, env=env)

# Runs the "generate" step.
def cmake_generate(args):
    cmake_args = ['-G', generator_name(args)]

    if args.platform == 'windows':
        if args.architecture == 'x86':
            cmake_args += ['-A', 'Win32']
        elif args.architecture == 'x64':
            cmake_args += ['-A', 'x64']
        elif args.architecture == 'arm64':
            cmake_args += ['-A', 'ARM64']
        elif args.architecture == 'arm64ec':
            cmake_args += ['-A', 'ARM64EC']

        if args.arm64x_pass is not None:
            # Both passes share the directory in which the ARM64 pass stores the paths of its linker inputs.
            arm64_pass_args = argparse.Namespace(**vars(args))
            arm64_pass_args.arm64x_pass = 'ARM64'
            link_dir = os.path.abspath(os.path.join('..', build_subdir(arm64_pass_args), 'arm64x-link'))
            cmake_args += ['-DSTEAMAUDIO_BUILD_AS_ARM64X=' + args.arm64x_pass]
            cmake_args += ['-DSTEAMAUDIO_ARM64X_LINK_DIR=' + link_dir.replace('\\', '/')]

    elif args.platform == 'linux':
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + config_name(args)]

        if args.architecture == 'x86':
            # For Linux x86, specify -m32 explicitly.
            cmake_args += ['-DCMAKE_C_FLAGS=-m32', '-DCMAKE_CXX_FLAGS=-m32', '-DCMAKE_SHARED_LINKER_FLAGS=-m32']
        elif args.architecture == 'arm64' and detect_host_architecture() != 'arm64':
            # Cross-compile from an x64 host. On an arm64 host, build natively.
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_linux_arm64.cmake']

    elif args.platform == 'osx':
        pass

    elif args.platform == 'android':
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + config_name(args)]

        if args.architecture == 'armv7':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_armv7.cmake']
        elif args.architecture == 'arm64':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_armv8.cmake']
        elif args.architecture == 'x86':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_x86.cmake']
        elif args.architecture == 'x64':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_x64.cmake']

        # For Android, point to the NDK installation using the ANDROID_NDK environment variable.
        if os.environ.get('ANDROID_NDK') is not None:
            cmake_args += ['-DCMAKE_ANDROID_NDK=' + os.environ.get('ANDROID_NDK')]
            cmake_args += ['-DCMAKE_MAKE_PROGRAM=' + os.environ.get('ANDROID_NDK') + '/prebuilt/windows-x86_64/bin/make.exe']

    elif args.platform == 'ios':
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_ios.cmake']

    elif args.platform == 'wasm':
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + os.environ.get('EMSDK') + '/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake']
        cmake_args += ['-DBUILD_SHARED_LIBS=FALSE']
        cmake_args += ['-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH']
        cmake_args += ['-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH']
        cmake_args += ['-DEMSCRIPTEN_SYSTEM_PROCESSOR=arm']
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + config_name(args)]

    # Install files to bin/.
    cmake_args += ['-DCMAKE_INSTALL_PREFIX=' + root_dir() + '/bin']

    if args.minimal:
        # For minimal builds, turn off various build options that add dependencies.
        cmake_args += ['-DSTEAMAUDIO_BUILD_ITESTS=FALSE']
        cmake_args += ['-DSTEAMAUDIO_BUILD_BENCHMARKS=FALSE']
        cmake_args += ['-DSTEAMAUDIO_BUILD_SAMPLES=FALSE']
        cmake_args += ['-DSTEAMAUDIO_BUILD_DOCS=FALSE']
        cmake_args += ['-DSTEAMAUDIO_ENABLE_IPP=FALSE']
        cmake_args += ['-DSTEAMAUDIO_ENABLE_EMBREE=FALSE']
        cmake_args += ['-DSTEAMAUDIO_ENABLE_RADEONRAYS=FALSE']
        cmake_args += ['-DSTEAMAUDIO_ENABLE_TRUEAUDIONEXT=FALSE']

    # If specified, enable unity builds.
    if args.unity:
        cmake_args += ['-DCMAKE_UNITY_BUILD=TRUE']

    cmake_args += ['../..']

    run_cmake('cmake', cmake_args)

# Runs the "build" step.
def cmake_build(args):
    cmake_args = ['--build', '.']

    if args.platform in ['windows', 'osx', 'ios']:
        cmake_args += ['--config', config_name(args)]

    # If specified, build using multiple threads.
    if args.parallel > 0:
        cmake_args += ['--parallel', str(args.parallel)]

    run_cmake('cmake', cmake_args)

# Runs the "test" step.
def cmake_test(args):
    cmake_args = []

    if args.platform in ['windows', 'osx', 'ios']:
        cmake_args += ['-C', config_name(args)]

    run_cmake('ctest', cmake_args)

# Runs the "install" step.
def cmake_install(args):
    cmake_args = ['--install', '.']

    if args.platform in ['windows', 'osx', 'ios']:
        cmake_args += ['--config', config_name(args)]

    run_cmake('cmake', cmake_args)

# Runs the "package" step.
def cmake_package(args):
    cmake_args = ['-G', 'ZIP']

    if args.platform in ['windows', 'osx', 'ios']:
        cmake_args += ['-C', config_name(args)]

    run_cmake('cpack', cmake_args)

# Finds a build tool (e.g. cmake, ispc).
def find_tool(name, dir_regex, min_version):
    matches = {}

    tools_dirs = [os.path.normpath(root_dir() + '/../../tools'), os.path.normpath(root_dir() + '/../tools')]
    for tools_dir in tools_dirs:
        if not os.path.exists(tools_dir):
            continue

        dir_contents = os.listdir(tools_dir)
        for item in dir_contents:
            dir_path = os.path.join(tools_dir, item)
            if not os.path.isdir(dir_path):
                continue

            dir_name = os.path.basename(dir_path)

            match = re.match(dir_regex, dir_name)
            if match is None:
                continue

            matches[dir_path] = match

    num_version_components = len(min_version)

    latest_version = []
    for i in range(1, num_version_components + 1):
        latest_version.append(0)

    latest_version_path = None

    for path in list(matches.keys()):
        match = matches[path]

        version = []
        for i in range(1, num_version_components + 1):
            version.append(int(match.group(i)))

        newer_version_found = False

        for i in range(1, num_version_components + 1):
            if version[i-1] < min_version[i-1]:
                break
            if version[i-1] < latest_version[i-1]:
                break
            newer_version_found = True

        if newer_version_found:
            latest_version = version
            latest_version_path = path

    if latest_version_path is not None:
        host_platform = detect_host_system()
        host_architecture = detect_host_architecture()
        subdirectories = []
        if host_platform == 'windows':
            # Windows on ARM can run x64 tools under emulation, so fall back to them if there is no native build.
            subdirectories = ['windows-' + host_architecture, 'windows-x64']
        elif host_platform == 'linux':
            subdirectories = ['linux-' + host_architecture]
        elif host_platform == 'osx':
            subdirectories = ['osx']

        for subdirectory in subdirectories:
            if os.path.exists(os.path.join(latest_version_path, subdirectory)):
                latest_version_path = os.path.join(latest_version_path, subdirectory)
                break
        else:
            if len(subdirectories) > 0:
                latest_version_path = os.path.join(latest_version_path, subdirectories[0])

    return latest_version_path

# Returns the build passes to run. Windows ARM64X builds consist of an ARM64 pass, followed by an ARM64EC pass that
# links the outputs of both into ARM64X binaries. Other builds consist of a single pass.
def build_passes(args):
    if args.platform == 'windows' and args.architecture == 'arm64x':
        arm64_pass_args = argparse.Namespace(**vars(args))
        arm64_pass_args.architecture = 'arm64'
        arm64_pass_args.arm64x_pass = 'ARM64'
        arm64ec_pass_args = argparse.Namespace(**vars(args))
        arm64ec_pass_args.architecture = 'arm64ec'
        arm64ec_pass_args.arm64x_pass = 'ARM64EC'
        return [arm64_pass_args, arm64ec_pass_args]
    else:
        args.arm64x_pass = None
        return [args]

# Returns the steps to run for the given operation.
def operation_steps(args):
    steps = {
        'generate': ['generate'],
        'build': ['build'],
        'test': ['test'],
        'install': ['install'],
        'package': ['package'],
        'default': ['generate', 'build'],
        'ci_build': ['generate', 'build', 'install'],
        'ci_package': ['generate', 'package'],
    }[args.operation]

    if args.arm64x_pass == 'ARM64':
        # Only the ARM64EC pass produces the final (ARM64X) binaries, so only it is installed and packaged. It can only
        # be generated once the ARM64 pass has been built, though.
        steps = [step for step in steps if step not in ['install', 'package']]
        if 'generate' in steps and 'build' not in steps:
            steps.append('build')

    return steps

# Main script.

host_system = detect_host_system()
args = parse_command_line(host_system)

# CI defaults.
if args.operation == 'ci_build':
    args.unity = True
    args.parallel = 4

for pass_args in build_passes(args):
    try:
        os.makedirs(build_subdir(pass_args))
    except Exception as e:
        pass

    olddir = os.getcwd()
    os.chdir(build_subdir(pass_args))

    cmake_path = find_tool('cmake', r'cmake-(\d+)\.(\d+)\.?(\d+)?', [3, 17])
    if cmake_path is not None:
        if host_system == 'osx':
            os.environ['PATH'] = os.path.normpath(os.path.join(cmake_path, 'CMake.app', 'Contents', 'bin')) + os.pathsep + os.environ['PATH']
        else:
            os.environ['PATH'] = os.path.normpath(os.path.join(cmake_path, 'bin')) + os.pathsep + os.environ['PATH']

    for step in operation_steps(pass_args):
        if step == 'generate':
            cmake_generate(pass_args)
        elif step == 'build':
            cmake_build(pass_args)
        elif step == 'test':
            cmake_test(pass_args)
        elif step == 'install':
            cmake_install(pass_args)
        elif step == 'package':
            cmake_package(pass_args)

    os.chdir(olddir)
