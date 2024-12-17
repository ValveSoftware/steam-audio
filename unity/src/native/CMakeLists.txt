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

include(SteamAudioHelpers)

#
# VERSION STAMPING
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/steamaudio_unity_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/steamaudio_unity_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/audioplugin_phonon.rc.in ${CMAKE_CURRENT_BINARY_DIR}/audioplugin_phonon.rc)


#
# NATIVE AUDIO PLUGIN
#

set(SRC_UNITYNATIVE
    pch.h
    steamaudio_unity_version.h.in
    steamaudio_unity_native.h
    steamaudio_unity_native.cpp
    spatialize_effect.cpp
    ambisonic_decoder_effect.cpp
    reverb_effect.cpp
    mix_return_effect.cpp
)

if (IPL_OS_WINDOWS)
    list(APPEND SRC_UNITYNATIVE audioplugin_phonon.rc.in ${CMAKE_CURRENT_BINARY_DIR}/audioplugin_phonon.rc)
endif()

if (BUILD_SHARED_LIBS AND NOT IPL_OS_IOS)
    if (IPL_OS_MACOS)
        add_library(audioplugin_phonon MODULE ${SRC_UNITYNATIVE})
        set_target_properties(audioplugin_phonon PROPERTIES BUNDLE TRUE)
    else()
        add_library(audioplugin_phonon SHARED ${SRC_UNITYNATIVE})
    endif()
else()
    add_library(audioplugin_phonon STATIC ${SRC_UNITYNATIVE})
endif()

# This is needed so we can include headers as <ipp/ipp.h> instead of <ipp.h>
target_include_directories(audioplugin_phonon PRIVATE ${CMAKE_HOME_DIRECTORY}/include ${CMAKE_CURRENT_SOURCE_DIR})

# This is needed so we can include generated headers
target_include_directories(audioplugin_phonon PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (IPL_OS_WINDOWS)
    target_link_libraries(audioplugin_phonon PRIVATE delayimp)
elseif (IPL_OS_LINUX)
    target_link_libraries(audioplugin_phonon PRIVATE m dl pthread)
elseif (IPL_OS_ANDROID)
    target_link_libraries(audioplugin_phonon PRIVATE log android)
endif()

target_link_libraries(audioplugin_phonon PRIVATE Unity::NativeAudio SteamAudio::SteamAudio)

target_precompile_headers(audioplugin_phonon PRIVATE pch.h)

if (IPL_OS_LINUX AND BUILD_SHARED_LIBS AND (NOT IPL_CPU_ARMV8))
    add_custom_command(
        TARGET      audioplugin_phonon
        POST_BUILD
        COMMAND     objcopy --only-keep-debug $<TARGET_FILE_NAME:audioplugin_phonon> $<TARGET_FILE_NAME:audioplugin_phonon>.dbg
        COMMAND     strip --strip-debug --strip-unneeded $<TARGET_FILE_NAME:audioplugin_phonon>
        COMMAND     objcopy --add-gnu-debuglink=$<TARGET_FILE_NAME:audioplugin_phonon>.dbg $<TARGET_FILE_NAME:audioplugin_phonon>
        COMMENT     "Extracting debug symbols..."
    )
endif()

if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86/)
    elseif (IPL_CPU_X64)
        set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64/)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X86)
        set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86/)
    elseif (IPL_CPU_X64)
        set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86_64/)
    endif()
elseif (IPL_OS_MACOS)
    set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS/)
elseif(IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/armv7/)
    elseif (IPL_CPU_ARMV8)
        set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/arm64/)
    elseif(IPL_CPU_X86)
        set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/x86/)
    endif()
elseif (IPL_OS_IOS)
    set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/iOS/)
elseif (IPL_OS_WASM)
    set(PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/HTML5/)
endif()

if (IPL_OS_MACOS)
    # CMake's Xcode generator runs post-build steps before code signing, so the copied files don't have the signature applied.
    # This causes issues in newer versions of macOS, which refuse to load .bundles that don't have a signature.
    add_custom_target(audioplugin_phonon_copy ALL
        COMMAND     ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:audioplugin_phonon> ${PLUGIN_DIR}audioplugin_phonon.bundle
        VERBATIM
    )
    add_dependencies(audioplugin_phonon_copy audioplugin_phonon)
else()
    add_custom_command(
        TARGET      audioplugin_phonon
        POST_BUILD
        COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:audioplugin_phonon> ${PLUGIN_DIR}
    )
endif()


#
# INSTALL
#

get_bin_subdir(IPL_BIN_SUBDIR)

install(
    TARGETS             audioplugin_phonon
    ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
    LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
    RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
)

if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/audioplugin_phonon.pdb
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_LINUX AND NOT IPL_CPU_ARMV8)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/libaudioplugin_phonon.so.dbg
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
endif()
