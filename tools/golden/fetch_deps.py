"""Fetch and build the third-party dependencies needed to build the reference Steam Audio in core/.

A minimal replacement for core/build/get_dependencies.py, which only knows Visual Studio generators up to
VS 2022 and builds many optional libraries we do not use. This script:

- reads the pinned commits, patches and CMake flags from core/build/dependencies.json (single source of truth);
- builds only the dependencies the minimal reference configuration needs (no Embree, IPP, OpenCL, ...);
- uses the Ninja generator, so it works with any MSVC version as well as GCC/Clang;
- installs into core/deps/<name>/ in exactly the layout core/build/Find*.cmake expect.

On Windows, run it from an environment where cl.exe is on PATH (vcvarsall.bat x64). See tools/golden/README.md.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CORE = REPO / "core"
DEPS = CORE / "deps"
DEPS_BUILD = CORE / "deps-build"

# Order matters: mysofa links against zlib.
DEFAULT_DEPS = ["zlib", "mysofa", "pffft", "flatbuffers", "catch"]


def host_platform():
    if sys.platform == "win32":
        return "windows-x64"
    if sys.platform == "darwin":
        return "osx"
    return "linux-x64"


def run(cmd, cwd=None):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.check_call([str(c) for c in cmd], cwd=cwd)


def substitute(text, name, platform, config):
    return (
        text.replace("$deps", DEPS.as_posix())
        .replace("$src", (DEPS_BUILD / name / "src").as_posix())
        .replace("$platform", platform)
        .replace("$config", config)
    )


def cmake_flags(dep, name, platform, config):
    flags = []
    for layer in dep.get("configure", {}).get("cmake", []):
        if "platforms" in layer and platform not in layer["platforms"]:
            continue
        flags += [substitute(f, name, platform, config) for f in layer.get("flags", [])]
    return flags


def fetch(name, dep):
    src = DEPS_BUILD / name / "src" / name
    commit = dep["fetch"]["tag"]
    if (src / ".git").exists():
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=src, text=True).strip()
        if head == commit:
            print(f"{name}: source at {commit[:12]} already present")
            return src
        shutil.rmtree(src, onerror=lambda f, p, _: (os.chmod(p, 0o700), f(p)))

    src.mkdir(parents=True)
    run(["git", "init", "-q"], cwd=src)
    run(["git", "remote", "add", "origin", dep["fetch"]["git"]], cwd=src)
    run(["git", "fetch", "-q", "--depth", "1", "origin", commit], cwd=src)
    run(["git", "checkout", "-q", "FETCH_HEAD"], cwd=src)
    if "patch" in dep["fetch"]:
        run(["git", "apply", CORE / "build" / dep["fetch"]["patch"]], cwd=src)
    return src


def build(name, dep, src, platform, config, cmake, ninja):
    build_dir = DEPS_BUILD / name / "build" / platform
    install_dir = DEPS_BUILD / name / "install" / platform
    args = [
        cmake, "-G", "Ninja", "-S", src, "-B", build_dir,
        f"-DCMAKE_BUILD_TYPE={config.capitalize()}",
        f"-DCMAKE_INSTALL_PREFIX={install_dir.as_posix()}",
        # Matches upstream's STEAMAUDIO_STATIC_RUNTIME=ON.
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
        # Pinned upstream commits predate CMake 4, which dropped support for cmake_minimum_required < 3.5.
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        # Old cmake_minimum_required leaves CMP0091 at OLD, which ignores CMAKE_MSVC_RUNTIME_LIBRARY and links /MD.
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
    ]
    if ninja:
        args.append(f"-DCMAKE_MAKE_PROGRAM={ninja}")
    run(args + cmake_flags(dep, name, platform, config))

    target = dep.get("build", {}).get("cmake", {}).get("target")
    run([cmake, "--build", build_dir] + (["--target", target] if target else []))
    if dep.get("install"):
        run([cmake, "--install", build_dir])


def copy_outputs(name, dep, platform, config):
    for src_rel, dst_rel in dep.get("copy", []):
        src = DEPS_BUILD / name / substitute(src_rel, name, platform, config)
        dst = DEPS / name / substitute(dst_rel, name, platform, config)
        if not src.exists():
            raise FileNotFoundError(f"{name}: expected build output {src} is missing")
        print(f"{name}: {src} -> {dst}")
        shutil.copytree(src, dst, dirs_exist_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("deps", nargs="*", default=DEFAULT_DEPS, help=f"dependencies (default: {DEFAULT_DEPS})")
    parser.add_argument("--cmake", default="cmake", help="path to cmake")
    parser.add_argument("--ninja", help="path to ninja, if not on PATH")
    args = parser.parse_args()

    spec = json.loads((CORE / "build" / "dependencies.json").read_text())
    platform, config = host_platform(), "release"

    for name in args.deps:
        dep = spec[name]
        print(f"\n=== {name} ===", flush=True)
        src = fetch(name, dep)
        if "configure" in dep or "build" in dep:
            build(name, dep, src, platform, config, args.cmake, args.ninja)
        copy_outputs(name, dep, platform, config)

    print(f"\nDone. Dependencies installed into {DEPS}")


if __name__ == "__main__":
    main()
