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

# Support for building ARM64X binaries on Windows. An ARM64X DLL contains both ARM64 and ARM64EC code, so it can be
# loaded into ARM64 processes as well as x64 and ARM64EC processes on Windows on ARM. It is built in two passes (see
# build.py -a arm64x), which must use the same STEAMAUDIO_ARM64X_LINK_DIR:
#
#   1. An ARM64 build (STEAMAUDIO_BUILD_AS_ARM64X=ARM64), which records the absolute paths of the linker inputs of
#      each ARM64X target.
#   2. An ARM64EC build (STEAMAUDIO_BUILD_AS_ARM64X=ARM64EC), which links its own inputs together with the ARM64 ones
#      into ARM64X binaries.
#
# See https://learn.microsoft.com/windows/arm/arm64x-build.

function (steamaudio_add_arm64x_target TARGET)
    # Only linked binaries can be ARM64X.
    get_target_property(_type ${TARGET} TYPE)
    if (NOT _type MATCHES "^(SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE)$")
        return()
    endif()

    if (STEAMAUDIO_BUILD_AS_ARM64X STREQUAL "ARM64")
        file(MAKE_DIRECTORY ${STEAMAUDIO_ARM64X_LINK_DIR})
        # /LINKREPROFULLPATHRSP requires the linker from Visual Studio 2022 17.11 or later.
        target_link_options(${TARGET} PRIVATE "/LINKREPROFULLPATHRSP:${STEAMAUDIO_ARM64X_LINK_DIR}/${TARGET}.rsp")
    elseif (STEAMAUDIO_BUILD_AS_ARM64X STREQUAL "ARM64EC")
        set(_rsp_file ${STEAMAUDIO_ARM64X_LINK_DIR}/${TARGET}.rsp)
        if (NOT EXISTS ${_rsp_file})
            message(FATAL_ERROR "${_rsp_file} not found. Build the ARM64 pass of the ARM64X build first.")
        endif()

        file(STRINGS ${_rsp_file} _arm64_objs REGEX "obj\"$")
        file(STRINGS ${_rsp_file} _arm64_defs REGEX "def\"$")
        file(STRINGS ${_rsp_file} _arm64_libs REGEX "lib\"$")
        string(REPLACE "\"" ";" _arm64_objs "${_arm64_objs}")
        string(REPLACE "\"" ";" _arm64_defs "${_arm64_defs}")
        string(REPLACE "\"" ";" _arm64_libs "${_arm64_libs}")
        string(REPLACE "/def:" "/defArm64Native:" _arm64_defs "${_arm64_defs}")

        set(_arm64_obj_paths "")
        foreach (_obj ${_arm64_objs})
            string(REPLACE "\\" "/" _obj_path "${_obj}")
            list(APPEND _arm64_obj_paths ${_obj_path})
        endforeach()

        target_sources(${TARGET} PRIVATE ${_arm64_obj_paths})
        target_link_options(${TARGET} PRIVATE /machine:arm64x ${_arm64_defs} ${_arm64_libs})
    endif()
endfunction()
