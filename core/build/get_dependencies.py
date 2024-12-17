import argparse
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

# ----------------------------------------------------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------------------------------------------------

def change_dir(dir):
    # print('change directory:', dir)
    os.chdir(dir)

def make_dir(dir):
    if not os.path.exists(dir):
        # print('make directory:', dir)
        os.makedirs(dir, exist_ok=True)

def move_file_or_dir(src, dest):
    # print('move', src, 'to', dest)
    shutil.move(src, dest)

def rmtree_remove_readonly(func, path, _):
    os.chmod(path, stat.S_IWRITE)
    func(path)

def delete_directory(dir):
    # print('delete directory', dir)
    shutil.rmtree(dir, onerror=rmtree_remove_readonly)

def env_set(name, value):
    # print('set env var', name, 'to', value)
    os.environ[name] = value

def run_command(command, dir=None):
    cur_dir = os.getcwd()
    if dir is not None:
        change_dir(dir)

    print('run command:', ' '.join(command))
    subprocess.check_call(command)

    change_dir(cur_dir)

def vs_generator_name(version):
    if version == 2013:
        return 'Visual Studio 12 2013'
    elif version == 2015:
        return 'Visual Studio 14 2015'
    elif version == 2017:
        return 'Visual Studio 15 2017'
    elif version == 2019:
        return 'Visual Studio 16 2019'
    elif version == 2022:
        return 'Visual Studio 17 2022'
    else:
        return 'unknown'


# ----------------------------------------------------------------------------------------------------------------------
# Clean Stage
# ----------------------------------------------------------------------------------------------------------------------

def clean(type):
    if type in ['output', 'all']:
        root = os.path.join(os.getcwd(), 'deps')
        if os.path.exists(root):
            for subdir in os.listdir(root):
                if os.path.exists(os.path.join(root, subdir, 'stamp', '.fetchinfo')):
                    delete_directory(os.path.join(root, subdir))

    if type in ['build', 'all']:
        root = os.path.join(os.getcwd(), 'deps-build')
        if os.path.exists(root):
            for subdir in os.listdir(root):
                for stage in ['build', 'install', 'stamp']:
                    if os.path.exists(os.path.join(root, subdir, stage)):
                        delete_directory(os.path.join(root, subdir, stage))

    if type in ['src', 'all']:
        root = os.path.join(os.getcwd(), 'deps-build')
        if os.path.exists(root):
            for subdir in os.listdir(root):
                if os.path.exists(os.path.join(root, subdir, 'src')):
                    delete_directory(os.path.join(root, subdir, 'src'))


# ----------------------------------------------------------------------------------------------------------------------
# Filter Stage
# ----------------------------------------------------------------------------------------------------------------------

def filter_is_tool(dep):
    is_tool = dep.get("tool", False)
    if is_tool:
        print('tool dependency, ensuring availability for host platform')
    return is_tool

def filter_get_platform(dep, host_platform, target_platform):
    is_tool = filter_is_tool(dep)
    return host_platform if is_tool else target_platform

def filter_dependency(dep, platform):
    platforms = dep.get("platforms", [])
    if len(platforms) > 0:
        if platform == "android-armv8":
            return "android-arm64" in platforms
        else:
            return platform in platforms
    else:
        # nothing specified, so supported on all platforms
        return True


# ----------------------------------------------------------------------------------------------------------------------
# Stamping
# ----------------------------------------------------------------------------------------------------------------------

def stamp_write(name, platform, params, file_name):
    stamp_dir = os.path.join(os.getcwd(), 'deps-build', name, 'stamp')
    stamp_platform_dir = os.path.join(stamp_dir, platform)
    make_dir(stamp_platform_dir)

    stamp_string = ';'.join(params)

    stamp_file_name = os.path.join(stamp_dir, file_name)
    # print('write', stamp_string, 'to', stamp_file_name)
    with open(stamp_file_name, 'w') as stamp_file:
        stamp_file.write(stamp_string)

def stamp_fetch(name, platform, fetch_params):
    stamp_write(name, platform, fetch_params, '.fetchinfo')

def stamp_configure(name, platform, configure_params):
    stamp_write(name, platform, configure_params, os.path.join(platform, '.configureinfo'))

def stamp_copy(name, platform):
    stamp_dir = os.path.join(os.getcwd(), 'deps-build', name, 'stamp')
    stamp_platform_dir = os.path.join(stamp_dir, platform)

    stamp_out_dir = os.path.join(os.getcwd(), 'deps', name, 'stamp')
    stamp_out_platform_dir = os.path.join(os.getcwd(), 'deps', name, 'stamp', platform)
    make_dir(stamp_out_platform_dir)

    fetch_stamp_src = os.path.join(stamp_dir, '.fetchinfo')
    configure_stamp_src = os.path.join(stamp_platform_dir, '.configureinfo')

    if os.path.exists(fetch_stamp_src):
        copy_file(fetch_stamp_src, stamp_out_dir)

    if os.path.exists(configure_stamp_src):
        copy_file(configure_stamp_src, stamp_out_platform_dir)

def stamp_check(name, platform, fetch_params, configure_params=None):
    fetch_stamp_file = os.path.join(os.getcwd(), 'deps', name, 'stamp', '.fetchinfo')
    if not os.path.exists(fetch_stamp_file):
        print('fetch stamp not found in ' + name)
        return False
    with open(fetch_stamp_file, 'r') as file:
        if file.readline() != ';'.join(fetch_params):
            print('fetch stamp mismatch in ' + name)
            return False

    if configure_params is not None:
        configure_stamp_file = os.path.join(os.getcwd(), 'deps', name, 'stamp', platform, '.configureinfo')
        if not os.path.exists(configure_stamp_file):
            print('configure stamp not found in ' + name)
            return False
        with open(configure_stamp_file, 'r') as file:
            if file.readline() != ';'.join(configure_params):
                print('configure stamp mismatch in ' + name)
                return False

    return True


# ----------------------------------------------------------------------------------------------------------------------
# Placeholder Substitution
# ----------------------------------------------------------------------------------------------------------------------

def substitute_placeholders_in_string(string, name, platform, cmake, vs_version, debug):
    result = string

    # $platform -> name of the platform
    result = result.replace('$platform', platform)

    # $src -> abs path to deps-build/(name)/src
    result = result.replace('$src', os.path.join(os.getcwd(), 'deps-build', name, 'src'))

    # $build -> abs path to deps-build/(name)/build/(platform)
    result = result.replace('$build', os.path.join(os.getcwd(), 'deps-build', name, 'build', platform))

    # $install -> abs path to deps-build/(name)/install/(platform)
    result = result.replace('$install', os.path.join(os.getcwd(), 'deps-build', name, 'install', platform))

    # $deps -> abs path to deps
    result = result.replace('$deps', os.path.join(os.getcwd(), 'deps'))

    # $cmake -> abs path to cmake.exe
    result = result.replace('$cmake', cmake)

    # $vs -> substituted with vs generator name
    result = result.replace('$vs', vs_generator_name(vs_version))

    # $config -> debug/release
    result = result.replace('$config', 'debug' if debug else 'release')
    result = result.replace('$Config', 'Debug' if debug else 'Release')

    # $msbuild:nnnn -> find and substitute with abs path to msbuild.exe
    regex = r'(\$msbuild):(\d\d\d\d)'
    match = re.search(regex, result)
    if match:
        # get the specified msbuild version
        msbuild_version = int(match.group(2))

        # see if we have the cmake output cached
        cache_file_name = 'cmake_msbuild_' + match.group(2) + '.txt'
        if not os.path.exists(os.path.join(os.getcwd(), cache_file_name)):
            # if not, run cmake to dump the location of msbuild to the cache file
            with open(cache_file_name, 'w') as cache_file:
                subprocess.check_call([cmake, '-G', vs_generator_name(msbuild_version), '--system-information'], stdout=cache_file, stderr=cache_file)

        # read the cached file to get the msbuild location
        msbuild_path = None
        with open(cache_file_name, 'r') as cache_file:
            lines = cache_file.readlines()
            for line in lines:
                if line.startswith('CMAKE_MAKE_PROGRAM'):
                    msbuild_path = line.replace('CMAKE_MAKE_PROGRAM', '', 1).strip().lstrip('\"').rstrip('\"')
                    break

        # substitute in the string
        if msbuild_path is not None:
            result = result.replace(match.group(0), msbuild_path)
        else:
            print('ERROR: unable to find msbuild for Visual Studio ' + match.group(2))
            exit(1)

    return result

def substitute_placeholders(value, name, platform, cmake, vs_version, debug):
    if isinstance(value, dict):
        result = {}
        for key in value.keys():
            result[key] = substitute_placeholders(value[key], name, platform, cmake, vs_version, debug)
        return result
    elif isinstance(value, list):
        return [substitute_placeholders(x, name, platform, cmake, vs_version, debug) for x in value]
    elif isinstance(value, str):
        return substitute_placeholders_in_string(value, name, platform, cmake, vs_version, debug)
    else:
        return value


# ----------------------------------------------------------------------------------------------------------------------
# Check Stage
# ----------------------------------------------------------------------------------------------------------------------

def find_file(dep_name, subdir, file_name):
    file_path = os.path.join(os.getcwd(), 'deps', dep_name, subdir, file_name)
    found = os.path.exists(file_path)
    if found:
        print(file_path + ' - found')
    return found

def find_file_for_platform(dep_name, subdir, file_name, platform_name, debug):
    return find_file(dep_name, subdir, os.path.join(platform_name, file_name)) or find_file(dep_name, subdir, os.path.join(platform_name, ('debug' if debug else 'release'), file_name))

def get_static_library_name(name, platform):
    if platform.startswith('windows-'):
        return name + '.lib'
    else:
        return 'lib' + name + '.a'

def get_shared_library_name(name, platform):
    if platform.startswith('windows-'):
        return name + '.dll'
    elif platform in ['osx', 'ios']:
        return 'lib' + name + 'dylib'
    else:
        return 'lib' + name + '.a'

def get_program_name(name, platform):
    if platform.startswith('windows-'):
        return name + '.exe'
    else:
        return name

def get_fetch_stamp(dep, platform):
    fetch = dep.get("fetch", {})

    git_url = fetch.get("git", None)
    git_tag = fetch.get("tag", None)
    if git_url is not None and git_tag is not None:
        return [git_url, git_tag]

    url = fetch.get("url", None)
    if url is not None:
        if isinstance(url, dict):
            return [url.get(x) for x in sorted(url.keys())]
        if url is not None:
            return [url]

    return []

def get_configure_stamp(dep, platform):
    configure = dep.get("configure", {})

    cmake_options = configure.get("cmake", None)
    if cmake_options is not None:
        stamp = []
        for layer in cmake_options:
            should_stamp = layer.get("stamp", True)
            if should_stamp:
                platforms = layer.get("platforms", None)
                if platforms is not None and platform not in platforms:
                    continue

                stamp += layer.get("flags", [])

        return stamp

    custom_options = configure.get("custom", None)
    if custom_options is not None:
        return custom_options.get("stamp", [])

    return None

# return true if the dependency is already satisfied
def check_dependency(name, dep, platform, debug):
    check = dep.get("check", {})
    if len(check) == 0:
        return True

    headers = check.get("headers", [])
    static_libraries = check.get("static_libraries", [])
    shared_libraries = check.get("shared_libraries", [])
    programs = check.get("programs", [])

    alt_platform_names = dep.get("alt_platform_names", {})
    platform_name_to_check = alt_platform_names.get(platform, platform)

    for header in headers:
        if not find_file(name, 'include', header):
            return False

    for static_library in static_libraries:
        found = False
        for static_library_name in static_library:
            if find_file_for_platform(name, 'lib', get_static_library_name(static_library_name, platform), platform_name_to_check, debug):
                found = True
                break
        if not found:
            return False

    for shared_library in shared_libraries:
        found = False
        for shared_library_name in shared_library:
            if find_file_for_platform(name, 'bin', get_shared_library_name(shared_library_name, platform), platform_name_to_check, debug):
                found = True
                break
        if not found:
            return False

    for program in programs:
        found = False
        for program_name in program:
            if find_file_for_platform(name, 'bin', get_program_name(program_name, platform), platform_name_to_check, debug):
                found = True
                break
        if not found:
            return False

    fetch = dep.get("fetch", {})
    fail = fetch.get("fail", None)
    should_check_stamp = False if fail is not None else True
    if should_check_stamp and not stamp_check(name, platform, get_fetch_stamp(dep, platform), get_configure_stamp(dep, platform)):
        return False

    return True


# ----------------------------------------------------------------------------------------------------------------------
# Fetch Stage
# ----------------------------------------------------------------------------------------------------------------------

def fetch_git(name, url, tag, patch, platform):
    src_dir = os.path.join(os.getcwd(), 'deps-build', name, 'src')
    git_dir = os.path.join(src_dir, name)

    make_dir(src_dir)
    if not os.path.exists(git_dir):
        run_command(['git', 'clone', url, name], src_dir)
        run_command(['git', 'checkout', tag], git_dir)

    if patch is not None:
        patch_file_path = os.path.join(os.getcwd(), 'build', patch)
        run_command(['git', 'reset', '--hard', 'HEAD'], git_dir)
        run_command(['git', 'apply', '--ignore-whitespace', patch_file_path], git_dir)

    stamp_fetch(name, platform, [url, tag])

def fetch_url(name, url, prefix, platform):
    fetch_stamp = ''
    if isinstance(url, dict):
        fetch_stamp = [url.get(x) for x in sorted(url.keys())]
        url = url.get(platform, None)
    else:
        fetch_stamp = [url]

    if url is not None:
        download_dir = os.path.join(os.getcwd(), 'deps-build', name, 'src')
        extract_dir = os.path.join(os.getcwd(), 'deps-build', name, 'build', platform)

        file_name = os.path.basename(url)
        file_path = os.path.join(download_dir, file_name)

        make_dir(download_dir)
        print('downloading', url, 'to', file_path)
        urllib.request.urlretrieve(url, file_path)

        if not os.path.exists(extract_dir):
            make_dir(extract_dir)
            if file_name.endswith('.zip'):
                print('extracting (zip)', file_path, 'to', extract_dir)
                with zipfile.ZipFile(file_path) as zip_file:
                    zip_file.extractall(extract_dir)
            else:
                print('extracting (tar)', file_path, 'to', extract_dir)
                with tarfile.open(file_path) as tar_file:
                    tar_file.extractall(extract_dir)

            if len(prefix) > 0:
                prefix = prefix.get(platform, None)
                if prefix is not None:
                    prefix_dir = os.path.join(extract_dir, prefix)
                    # print('move contents of', prefix_dir, 'to', extract_dir)
                    if os.path.exists(prefix_dir):
                        for prefix_dir_entry in os.listdir(prefix_dir):
                            prefix_dir_entry_path = os.path.join(prefix_dir, prefix_dir_entry)
                            move_file_or_dir(prefix_dir_entry_path, extract_dir)

        stamp_fetch(name, platform, fetch_stamp)

def fetch_fail(fail):
    raise Exception(fail)

def fetch_dependency(name, dep, platform):
    fetch = dep.get("fetch", {})
    if len(fetch) == 0:
        return

    git_url = fetch.get("git", None)
    git_tag = fetch.get("tag", None)
    git_patch = fetch.get("patch", None)
    url = fetch.get("url", None)
    prefix = fetch.get("prefix", {})
    fail = fetch.get("fail", None)

    if git_url is not None and git_tag is not None:
        fetch_git(name, git_url, git_tag, git_patch, platform)
    elif url is not None:
        fetch_url(name, url, prefix, platform)
    else:
        fetch_fail(fail)


# ----------------------------------------------------------------------------------------------------------------------
# Configure Stage
# ----------------------------------------------------------------------------------------------------------------------

def toolchain(suffix):
    return os.path.join(os.getcwd(), 'build', 'toolchain_' + suffix + '.cmake')

def configure_cmake(name, cmake_layers, platform, cmake, vs_version, ndk_path, emsdk_path, debug, shared_crt):
    src_dir = os.path.join(os.getcwd(), 'deps-build', name, 'src', name)
    build_dir = os.path.join(os.getcwd(), 'deps-build', name, 'build', platform)
    install_dir = os.path.join(os.getcwd(), 'deps-build', name, 'install', platform)
    make_dir(build_dir)
    make_dir(install_dir)

    cmake_args = []

    if platform == 'windows-x86':
        cmake_args += ['-G', vs_generator_name(vs_version), '-A', 'Win32']
        if not shared_crt:
            cmake_args += ['-DCMAKE_C_FLAGS_RELEASE=/MT', '-DCMAKE_CXX_FLAGS_RELEASE=/MT']
            cmake_args += ['-DCMAKE_C_FLAGS_DEBUG=/MTd', '-DCMAKE_CXX_FLAGS_DEBUG=/MTd']
        else:
            cmake_args += ['-DCMAKE_C_FLAGS_RELEASE=/MD', '-DCMAKE_CXX_FLAGS_RELEASE=/MD']
            cmake_args += ['-DCMAKE_C_FLAGS_DEBUG=/MDd', '-DCMAKE_CXX_FLAGS_DEBUG=/MDd']
    elif platform == 'windows-x64':
        cmake_args += ['-G', vs_generator_name(vs_version), '-A', 'x64']
        if not shared_crt:
            cmake_args += ['-DCMAKE_C_FLAGS_RELEASE=/MT', '-DCMAKE_CXX_FLAGS_RELEASE=/MT']
            cmake_args += ['-DCMAKE_C_FLAGS_DEBUG=/MTd', '-DCMAKE_CXX_FLAGS_DEBUG=/MTd']
        else:
            cmake_args += ['-DCMAKE_C_FLAGS_RELEASE=/MD', '-DCMAKE_CXX_FLAGS_RELEASE=/MD']
            cmake_args += ['-DCMAKE_C_FLAGS_DEBUG=/MDd', '-DCMAKE_CXX_FLAGS_DEBUG=/MDd']
    elif platform == 'windows-arm64':
        cmake_args += ['-G', vs_generator_name(vs_version), '-A', 'ARM64']
        if not shared_crt:
            cmake_args += ['-DCMAKE_C_FLAGS_RELEASE=/MT', '-DCMAKE_CXX_FLAGS_RELEASE=/MT']
            cmake_args += ['-DCMAKE_C_FLAGS_DEBUG=/MTd', '-DCMAKE_CXX_FLAGS_DEBUG=/MTd']
        else:
            cmake_args += ['-DCMAKE_C_FLAGS_RELEASE=/MD', '-DCMAKE_CXX_FLAGS_RELEASE=/MD']
            cmake_args += ['-DCMAKE_C_FLAGS_DEBUG=/MDd', '-DCMAKE_CXX_FLAGS_DEBUG=/MDd']
    elif platform == 'linux-x86':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_C_FLAGS=-m32', '-DCMAKE_CXX_FLAGS=-m32', '-DCMAKE_SHARED_LINKER_FLAGS=-m32', '-DCMAKE_EXE_LINKER_FLAGS=-m32']
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]
        cmake_args += ['-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE']
    elif platform == 'linux-x64':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]
        cmake_args += ['-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE']
    elif platform == 'linux-arm64':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + toolchain('linux_arm64')]
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]
        cmake_args += ['-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE']
    elif platform == 'osx':
        cmake_args += ['-G', 'Xcode']
        cmake_args += ['-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64']
    elif platform == 'android-armv7':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + toolchain('android_armv7')]
        cmake_args += ['-DCMAKE_ANDROID_NDK=' + ndk_path]
        cmake_args += ['-DCMAKE_MAKE_PROGRAM=' + os.path.join(ndk_path, 'prebuilt', 'windows-x86_64', 'bin', 'make.exe')]
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]
        cmake_args += ['-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE']
    elif platform == 'android-armv8':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + toolchain('android_armv8')]
        cmake_args += ['-DCMAKE_ANDROID_NDK=' + ndk_path]
        cmake_args += ['-DCMAKE_MAKE_PROGRAM=' + os.path.join(ndk_path, 'prebuilt', 'windows-x86_64', 'bin', 'make.exe')]
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]
        cmake_args += ['-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE']
    elif platform == 'android-x86':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + toolchain('android_x86')]
        cmake_args += ['-DCMAKE_ANDROID_NDK=' + ndk_path]
        cmake_args += ['-DCMAKE_MAKE_PROGRAM=' + os.path.join(ndk_path, 'prebuilt', 'windows-x86_64', 'bin', 'make.exe')]
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]
        cmake_args += ['-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE']
    elif platform == 'android-x64':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + toolchain('android_x64')]
        cmake_args += ['-DCMAKE_ANDROID_NDK=' + ndk_path]
        cmake_args += ['-DCMAKE_MAKE_PROGRAM=' + os.path.join(ndk_path, 'prebuilt', 'windows-x86_64', 'bin', 'make.exe')]
        cmake_args += ['-DCMAKE_FIND_LIBRARY_CUSTOM_LIB_SUFFIX=64']
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]
        cmake_args += ['-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE']
    elif platform == 'ios':
        cmake_args += ['-G', 'Xcode']
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + toolchain('ios')]
    elif platform == 'wasm':
        cmake_args += ['-G', 'Unix Makefiles']
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + os.path.join(emsdk_path, 'upstream', 'emscripten', 'cmake', 'Modules', 'Platform', 'Emscripten.cmake')]
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + ('Debug' if debug else 'Release')]

    stamp = []

    if cmake_layers is not None:
        for cmake_layer in cmake_layers:
            layer_platforms = cmake_layer.get("platforms", None)
            if layer_platforms is not None and len(layer_platforms) > 0:
                if platform not in layer_platforms:
                    continue

            layer_flags = cmake_layer.get("flags", None)
            if layer_flags is not None:
                cmake_args += layer_flags

            should_stamp = cmake_layer.get("stamp", True)
            if should_stamp:
                stamp += layer_flags

    cmake_args += ['-DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=TRUE']
    cmake_args += ['-DCMAKE_INSTALL_PREFIX=' + install_dir]
    cmake_args += ['-S', src_dir, '-B', build_dir]

    run_command([cmake] + cmake_args)

    stamp_configure(name, platform, stamp)

def configure_custom(name, custom, platform, cmake, vs_version):
    env = custom.get("env", {})
    working_directory = custom.get("working_directory", None)
    commands = custom.get("commands", [])
    stamp = custom.get("stamp", [])

    old_env = {}
    for variable in env.keys():
        old_env[variable] = os.environ.get(variable, '')
        env_set(variable, env[variable])

    old_cwd = os.getcwd()
    if working_directory is not None:
        change_dir(working_directory)

    for command in commands:
        run_command(command)

    if working_directory is not None:
        change_dir(old_cwd)

    for variable in env.keys():
        env_set(variable, old_env[variable])

    stamp_configure(name, platform, stamp)

def configure_dependency(name, dep, platform, cmake, vs_version, ndk_path, emsdk_path, debug, shared_crt):
    configure = dep.get("configure", {})
    if len(configure) == 0:
        return

    cmake_layers = configure.get("cmake", None)
    custom = configure.get("custom", None)

    if cmake_layers is not None:
        configure_cmake(name, cmake_layers, platform, cmake, vs_version, ndk_path, emsdk_path, debug, shared_crt)
    elif custom is not None:
        configure_custom(name, custom, platform, cmake, vs_version)


# ----------------------------------------------------------------------------------------------------------------------
# Build Stage
# ----------------------------------------------------------------------------------------------------------------------

def build_cmake(name, cmake_options, platform, cmake, debug):
    target = cmake_options.get("target", None)

    build_dir = os.path.join(os.getcwd(), 'deps-build', name, 'build', platform)

    cmake_args = ['--build', build_dir]

    if platform.startswith('windows-') or platform in ['osx', 'ios']:
        cmake_args += ['--config', 'Debug' if debug else 'Release']

    if target is not None:
        cmake_args += ['--target', target]

    run_command([cmake] + cmake_args)

def build_custom(name, custom, platform):
    working_directory = custom.get("working_directory", None)
    commands = custom.get("commands", [])

    old_cwd = os.getcwd()
    if working_directory is not None:
        change_dir(working_directory)

    for command in commands:
        run_command(command)

    if working_directory is not None:
        change_dir(old_cwd)

def build_dependency(name, dep, platform, cmake, vs_version, ndk_path, debug):
    build = dep.get("build", {})
    if len(build) == 0:
        return

    cmake_options = build.get("cmake", None)
    custom_options = build.get("custom", None)

    if cmake_options is not None:
        build_cmake(name, cmake_options, platform, cmake, debug)
    elif custom_options is not None:
        build_custom(name, custom_options, platform)


# ----------------------------------------------------------------------------------------------------------------------
# Install Stage
# ----------------------------------------------------------------------------------------------------------------------

def install_dependency(name, dep, platform, cmake, debug):
    install = dep.get("install", False)
    if not install:
        return

    build_dir = os.path.join(os.getcwd(), 'deps-build', name, 'build', platform)

    cmake_args = ['--install', build_dir]

    if platform.startswith('windows-') or platform in ['osx', 'ios']:
        cmake_args += ['--config', 'Debug' if debug else 'Release']

    run_command([cmake] + cmake_args)


# ----------------------------------------------------------------------------------------------------------------------
# Copy Stage
# ----------------------------------------------------------------------------------------------------------------------

def copy_file(src, dest):
    make_dir(dest)
    shutil.copy(src, dest)

def copy_directory(src, dest):
    if os.path.exists(dest):
        delete_directory(dest)
    shutil.copytree(src, dest)

def copy_dependency(name, dep, platform):
    copy_list = dep.get("copy", [])

    for copy_item in copy_list:
        src = os.path.join(os.getcwd(), 'deps-build', name, copy_item[0])
        dest = os.path.join(os.getcwd(), 'deps', name, copy_item[1])

        if os.path.exists(src):
            if '.' not in src or os.path.isdir(src):
                copy_directory(src, dest)
            else:
                copy_file(src, dest)
        elif copy_item[0].startswith('install/') and copy_item[0].endswith('/lib'):
            # try lib64 instead of lib
            src = src.replace('/lib', '/lib64')
            if os.path.exists(src):
                if '.' not in src or os.path.isdir(src):
                    copy_directory(src, dest)
                else:
                    copy_file(src, dest)

    stamp_copy(name, platform)


# ----------------------------------------------------------------------------------------------------------------------
# Sort dependencies
# ----------------------------------------------------------------------------------------------------------------------

def toposort_r(i, deps, names, status, result):
    if status[i] == 2:
        return

    if status[i] == 1:
        return

    status[i] = 1

    name = names[i]
    dep = deps[name]
    deps_of_dep = dep.get('depends', [])
    for d in deps_of_dep:
        for j in range(0, len(names)):
            if names[j] == d:
                toposort_r(j, deps, names, status, result)
                break

    status[i] = 2

    result += [names[i]]

def toposort(deps):
    names = sorted(deps.keys())
    status = [0] * len(names)
    result = []

    for i in range(0, len(names)):
        toposort_r(i, deps, names, status, result)

    return result


# ----------------------------------------------------------------------------------------------------------------------
# Main Program
# ----------------------------------------------------------------------------------------------------------------------

old_dir = os.getcwd()
os.chdir('..')

# --- Introspection

if sys.platform == 'win32':
    host_os = 'windows'
elif sys.platform == 'darwin':
    host_os = 'osx'
else:
    host_os = 'linux'

if host_os == 'windows':
    host_platform = 'windows-x64'
elif host_os == 'linux':
    host_platform = 'linux-x64'
elif host_os == 'osx':
    host_platform = 'osx'

print('Host platform:', host_platform)


# --- Command-line parameters

parser = argparse.ArgumentParser()
parser.add_argument('-p', '--platform', help = "Target operating system.", choices = ['windows', 'osx', 'linux', 'android', 'ios', 'wasm'], type = str.lower, default = host_os)
parser.add_argument('-a', '--architecture', help = "CPU architecture.", choices = ['x86', 'x64', 'armv7', 'arm64'], type = str.lower, default = 'x64')
parser.add_argument('-t', '--toolchain', help = "Compiler toolchain. (Windows only)", choices = ['vs2013', 'vs2015', 'vs2017', 'vs2019', 'vs2022'], type = str.lower, default = 'vs2019')
parser.add_argument('--ndk', help = "Path to the Android NDK. (Android only)", default = os.getenv('ANDROID_NDK'))
parser.add_argument('--emsdk', help = "Path to the Emscripten SDK. (WebAssembly only)", default = os.getenv('EMSDK'))
parser.add_argument('--clean', help = "Dependencies or build products to clean.", type=str.lower, choices=['output', 'build', 'src', 'all'])
parser.add_argument('--dependency', help = "A single dependency to process.", type=str.lower)
parser.add_argument('--extra', help = "Also process extra optional dependencies.", action='store_true', default=False)
parser.add_argument('--debug', help = "Build debug binaries.", action='store_true', default=False)
parser.add_argument('--toolsonly', help = "Build tool dependencies only.", action='store_true', default=False)
parser.add_argument('--libsonly', help = "Build library dependencies only.", action='store_true', default=False)
parser.add_argument('--sharedcrt', help = "Link to shared C/C++ runtime library. (Windows only)", action='store_true', default=False)
args = parser.parse_args()

if args.platform in ['osx', 'ios', 'wasm']:
    target_platform = args.platform
elif args.platform == 'android' and args.architecture == 'arm64':
    target_platform = 'android-armv8'
else:
    target_platform = '-'.join([args.platform, args.architecture])

print('Target platform:', target_platform)

vs_version = int(args.toolchain.replace('vs', '', 1))
print('Visual Studio version:', vs_version)

if args.ndk is not None:
    print('Android NDK:', args.ndk)
else:
    if target_platform.startswith('android-'):
        print('ERROR: Must specify NDK path!')
        exit(1)

if args.clean is not None:
    print('Clean mode:', args.clean)
    clean(args.clean)
    exit(0)


# --- Environment variables

if target_platform.startswith('android-'):
    os.environ['ANDROID_NDK'] = args.ndk
    os.environ['PATH'] += os.pathsep + os.path.join(args.ndk, 'prebuilt', 'windows-x6_64', 'bin')
elif target_platform == 'ios':
    os.environ['SDKROOT'] = ''


# --- Find CMake

cmake = 'cmake'
dir = os.getcwd()
while True:
    suffix = 'CMake.app/Contents/bin' if host_platform == 'osx' else 'bin'
    if os.path.exists(os.path.join(dir, 'tools', 'cmake-3.25.2', host_platform, suffix)):
        # this is the path
        cmake_name = 'cmake.exe' if host_platform.startswith('windows-') else 'cmake'
        cmake = os.path.join(dir, 'tools', 'cmake-3.25.2', host_platform, suffix, cmake_name)
        break
    elif os.path.dirname(dir) == dir:
        # this is the root, stop
        break
    else:
        dir = os.path.dirname(dir)

print('CMake:', cmake)


# --- Read the JSON file

json_data = {}
with open('build/dependencies.json', 'r') as json_file:
    json_data = json.load(json_file)


# --- Process dependencies

succeeded = []
failed_required = {}
failed_optional = {}

ordered_dep_names = toposort(json_data)

for name in ordered_dep_names:
    # skip if we've explicitly requested a dependency on the command line, and this is not it
    if args.dependency is not None and name != args.dependency:
        continue

    dep = json_data[name]

    # skip if this is not a tool dependency, and we have passed --toolsonly on the command line
    if args.toolsonly and not dep.get('tool', False):
        continue
    elif args.libsonly and dep.get('tool', False):
        continue

    # skip if this dependency is marked "extra" and we haven't passed --extra on the command line
    if dep.get('type', 'optional') == 'extra' and not args.extra:
        continue

    # check if we should skip it based on platform
    if not filter_dependency(dep, target_platform):
        continue

    print('\nProcessing dependency:', name)

    platform = filter_get_platform(dep, host_platform, target_platform)

    debug = None if dep.get('tool', False) else args.debug

    dep = substitute_placeholders(dep, name, platform, cmake, vs_version, debug)

    # check if already satisfied
    if check_dependency(name, dep, platform, debug):
        continue

    start_dir = os.getcwd()

    try:
        fetch_dependency(name, dep, platform)
        configure_dependency(name, dep, platform, cmake, vs_version, args.ndk, args.emsdk, debug, args.sharedcrt)
        build_dependency(name, dep, platform, cmake, vs_version, args.ndk, debug)
        install_dependency(name, dep, platform, cmake, debug)
        copy_dependency(name, dep, platform)
    except Exception as e:
        print(e)
        if dep.get('type', 'optional') == 'required':
            failed_required[name] = e
        else:
            failed_optional[name] = e
        continue
    else:
        succeeded += [name]
    finally:
        os.chdir(start_dir)

print('\n\nSUMMARY\n')
print('The following dependencies were successfully added or updated:\n')
if len(succeeded) > 0:
    msg = ''
    for x in succeeded:
        msg += x + ' '
    print(msg)
print('\n')
if len(failed_optional) > 0:
    print('The following OPTIONAL dependencies failed (and can be ignored; Steam Audio can still be built, but with reduced functionality or performance):\n')
    for x in failed_optional.keys():
        print(x)
        print(failed_optional[x])
        print('')
if len(failed_required) > 0:
    print('The following REQUIRED dependencies failed (and must be fixed before Steam Audio can be built):\n')
    for x in failed_required.keys():
        print(x)
        print(failed_required[x])
        print('')

os.chdir(old_dir)

if (len(failed_optional) > 0) or (len(failed_required) > 0):
    exit(1)
