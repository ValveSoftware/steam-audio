# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

include(SteamAudioHelpers)

#
# VERSION STAMPING
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/steamaudio_fmod_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/steamaudio_fmod_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon_fmod.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmod.rc)


#
# AUDIO PLUGIN
#

set(SRC_FMOD
    pch.h
    steamaudio_fmod_version.h.in
    library.h
    library.cpp
    steamaudio_fmod.h
    steamaudio_fmod.cpp
    spatialize_effect.cpp
    reverb_effect.cpp
    mix_return_effect.cpp
    phonon_fmod.plugin.js
)

if (IPL_OS_WINDOWS)
    list(APPEND SRC_FMOD phonon_fmod.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmod.rc)
endif()

if (BUILD_SHARED_LIBS)
    add_library(phonon_fmod SHARED ${SRC_FMOD})
    if (IPL_OS_MACOS)
        add_library(phonon_fmod_bundle MODULE ${SRC_FMOD})
        set_target_properties(phonon_fmod_bundle PROPERTIES BUNDLE TRUE)
        set_target_properties(phonon_fmod_bundle PROPERTIES OUTPUT_NAME phonon_fmod)
    endif()
else()
    add_library(phonon_fmod STATIC ${SRC_FMOD})
endif()

# This is needed so we can include headers as <ipp/ipp.h> instead of <ipp.h>
target_include_directories(phonon_fmod PRIVATE ${CMAKE_HOME_DIRECTORY}/include ${CMAKE_CURRENT_SOURCE_DIR})
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_include_directories(phonon_fmod_bundle PRIVATE ${CMAKE_HOME_DIRECTORY}/include ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# This is needed so we can include generated headers
target_include_directories(phonon_fmod PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_include_directories(phonon_fmod_bundle PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

if (IPL_OS_WINDOWS)
    target_link_libraries(phonon_fmod PRIVATE delayimp)
elseif (IPL_OS_LINUX)
    target_link_libraries(phonon_fmod PRIVATE m dl pthread)
elseif (IPL_OS_ANDROID)
    target_link_libraries(phonon_fmod PRIVATE log android)
endif()

target_link_libraries(phonon_fmod PRIVATE FMOD::FMOD SteamAudio::SteamAudio)
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_link_libraries(phonon_fmod_bundle PRIVATE FMOD::FMOD SteamAudio::SteamAudio)
endif()

target_precompile_headers(phonon_fmod PRIVATE pch.h)
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_precompile_headers(phonon_fmod_bundle PRIVATE pch.h)
endif()


#
# POST-BUILD STEP FOR COPYING fmod -> unity
#

set(UNITY_FMOD_PLUGIN_DIR "")
if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/win/x86)
    elseif (IPL_CPU_X64)
        set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/win/x86_64)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X86)
        set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/linux/x86)
    elseif (IPL_CPU_X64)
        set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/linux/x86_64)
    endif()
elseif (IPL_OS_MACOS)
    set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/mac)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/android/armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/android/arm64-v8a)
    elseif (IPL_CPU_X86)
        set(UNITY_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/FMOD/lib/android/x86)
    endif()
endif()

if (NOT UNITY_FMOD_PLUGIN_DIR STREQUAL "")
    if (IPL_OS_MACOS)
        add_custom_command(
            TARGET      phonon_fmod_bundle
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:phonon_fmod_bundle> ${UNITY_FMOD_PLUGIN_DIR}/phonon_fmod.bundle
        )
    else()
        add_custom_command(
            TARGET      phonon_fmod
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon_fmod> ${UNITY_FMOD_PLUGIN_DIR}
        )
    endif()
endif()


#
# POST-BUILD STEP FOR COPYING fmod -> unreal
#

set(UNREAL_FMOD_PLUGIN_DIR "")
if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(UNREAL_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Win32)
    elseif (IPL_CPU_X64)
        set(UNREAL_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Win64)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X64)
        set(UNREAL_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Linux)
    endif()
elseif (IPL_OS_MACOS)
    set(UNREAL_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Mac)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(UNREAL_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(UNREAL_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/arm64-v8a)
    elseif (IPL_CPU_X86)
        set(UNREAL_FMOD_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/x86)
    endif()
endif()

if (NOT UNREAL_FMOD_PLUGIN_DIR STREQUAL "")
    add_custom_command(
        TARGET      phonon_fmod
        POST_BUILD
        COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon_fmod> ${UNREAL_FMOD_PLUGIN_DIR}
    )
endif()


#
# INSTALL
#

get_bin_subdir(IPL_BIN_SUBDIR)

install(
    TARGETS             phonon_fmod
    ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
    LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
    RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
)

if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    install(
        TARGETS             phonon_fmod_bundle
        ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
        LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
        RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
    )
endif()

if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/phonon_fmod.pdb
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
endif()

install(
    FILES       ${CMAKE_CURRENT_SOURCE_DIR}/phonon_fmod.plugin.js
    DESTINATION src
)

if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/phonon.dll
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_MACOS)
    install(
        FILES       ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/libphonon.dylib
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
    install(
        DIRECTORY   ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/phonon.bundle
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_LINUX OR IPL_OS_ANDROID)
    install(
        FILES       ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/libphonon.so
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
endif()