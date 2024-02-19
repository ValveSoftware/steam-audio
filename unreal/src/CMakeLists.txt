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

#
# VERSION STAMPING
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/SteamAudio.uplugin.in ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/SteamAudio.uplugin)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin.in ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin)

#
# UNREAL PLUGIN
#

set(UBT_ARGS "-iwyu -noubtmakefiles -nohotreload")

if (STEAMAUDIOUNREAL_TARGET_UE4)
    set(UE_TARGET_GAME UE4Game)
    set(UE_TARGET_EDITOR UE4Editor)
else()
    set(UE_TARGET_GAME UnrealGame)
    set(UE_TARGET_EDITOR UnrealEditor)
endif()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/HostProject.uproject "{ \"FileVersion\": 3, \"Plugins\": [ { \"Name\": \"SteamAudio\", \"Enabled\": true } ] }")

add_custom_target(phonon_unreal_setup
    COMMAND     cmake -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/HostProject
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProject
    COMMAND     cmake -E copy ${CMAKE_CURRENT_BINARY_DIR}/HostProject.uproject ${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio
    COMMAND     cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio
    COMMAND     cmake -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/Intermediate
    VERBATIM
)

add_custom_target(phonon_unreal_win32
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win32 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win32-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win32 Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win32-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_win64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_EDITOR} Win64 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_EDITOR}-Win64-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win64 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win64-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win64 Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win64-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_linux
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Linux Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Linux-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Linux Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Linux-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_mac
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_EDITOR} Mac Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_EDITOR}-Mac-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Mac Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Mac-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Mac Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Mac-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_android
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Development.xml ${UBT_ARGS} -architectures=armv7+arm64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Shipping.xml ${UBT_ARGS} -architectures=armv7+arm64
    VERBATIM
)

add_custom_target(phonon_unreal_ios
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} iOS Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-iOS-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} iOS Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-iOS-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal ALL
    COMMAND     cmake -E echo Built plugin SteamAudio.
    VERBATIM
)

add_dependencies(phonon_unreal_win32 phonon_unreal_setup)
add_dependencies(phonon_unreal_win64 phonon_unreal_setup)
add_dependencies(phonon_unreal_linux phonon_unreal_setup)
add_dependencies(phonon_unreal_mac phonon_unreal_setup)
add_dependencies(phonon_unreal_android phonon_unreal_setup)
add_dependencies(phonon_unreal_ios phonon_unreal_setup)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    add_dependencies(phonon_unreal phonon_unreal_win64)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_WIN32)
        add_dependencies(phonon_unreal phonon_unreal_win32)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        add_dependencies(phonon_unreal phonon_unreal_linux)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        add_dependencies(phonon_unreal phonon_unreal_android)
    endif()
elseif (IPL_OS_MACOS)
    add_dependencies(phonon_unreal phonon_unreal_mac)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS)
        add_dependencies(phonon_unreal phonon_unreal_ios)
    endif()
endif()

#
# UNREAL + FMOD STUDIO PLUGIN
#

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD.uproject "{ \"FileVersion\": 3, \"Plugins\": [ { \"Name\": \"SteamAudio\", \"Enabled\": true }, { \"Name\": \"FMODStudio\", \"Enabled\": true }, { \"Name\": \"SteamAudioFMODStudio\", \"Enabled\": true } ] }")

add_custom_target(phonon_unreal_fmod_setup
    COMMAND     cmake -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD
    COMMAND     cmake -E copy ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD.uproject ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudio
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/FMODStudio
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio
    COMMAND     cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudio
    COMMAND     cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/FMODStudio
    COMMAND     cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio
    COMMAND     cmake -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/Intermediate
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/FMODStudio/Binaries/Win64/${UE_TARGET_EDITOR}.modules
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/FMODStudio/Binaries/Win64/${UE_TARGET_EDITOR}-FMODStudio.dll
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/FMODStudio/Binaries/Win64/${UE_TARGET_EDITOR}-FMODStudio.pdb
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/FMODStudio/Binaries/Win64/${UE_TARGET_EDITOR}-FMODStudioEditor.dll
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/FMODStudio/Binaries/Win64/${UE_TARGET_EDITOR}-FMODStudioEditor.pdb
    VERBATIM
)

add_custom_target(phonon_unreal_fmod_win32
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win32 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win32-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win32 Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win32-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_fmod_win64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_EDITOR} Win64 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_EDITOR}-Win64-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win64 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win64-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win64 Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win64-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_fmod_linux
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Linux Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Linux-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Linux Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Linux-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_fmod_mac
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_EDITOR} Mac Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_EDITOR}-Mac-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Mac Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Mac-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Mac Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Mac-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_fmod_android
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Development.xml ${UBT_ARGS} -architectures=armv7+arm64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Shipping.xml ${UBT_ARGS} -architectures=armv7+arm64
    VERBATIM
)

add_custom_target(phonon_unreal_fmod_ios
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} iOS Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-iOS-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} iOS Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-iOS-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_fmod ALL
    COMMAND     cmake -E echo Built plugin SteamAudioFMODStudio.
    VERBATIM
)

add_dependencies(phonon_unreal_fmod_win32 phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_win64 phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_linux phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_mac phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_android phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_ios phonon_unreal_fmod_setup)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    add_dependencies(phonon_unreal_fmod phonon_unreal_fmod_win64)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_WIN32)
        add_dependencies(phonon_unreal_fmod phonon_unreal_fmod_win32)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        add_dependencies(phonon_unreal_fmod phonon_unreal_fmod_linux)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        add_dependencies(phonon_unreal_fmod phonon_unreal_fmod_android)
    endif()
elseif (IPL_OS_MACOS)
    add_dependencies(phonon_unreal_fmod phonon_unreal_fmod_mac)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS)
        add_definitions(phonon_unreal_fmod phonon_unreal_fmod_ios)
    endif()
endif()

add_dependencies(phonon_unreal_fmod phonon_unreal)

#
# INSTALL
#

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    install(CODE "execute_process(COMMAND cmake -E rm -r ${CMAKE_HOME_DIRECTORY}/bin/unreal/SteamAudio COMMAND_ECHO STDOUT)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/SteamAudio.uplugin DESTINATION unreal/SteamAudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/Source DESTINATION unreal/SteamAudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/Content DESTINATION unreal/SteamAudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/Resources DESTINATION unreal/SteamAudio)
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_EDITOR}-Win64-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win64-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win64-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_WIN32)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win32-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Win32-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Linux-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Linux-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/FMODStudio)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Win64/phonon_fmod.dll DESTINATION unreal/FMODStudio/Binaries/Win64)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_WIN32)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Win32/phonon_fmod.dll DESTINATION unreal/FMODStudio/Binaries/Win32)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Linux/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Linux)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/armeabi-v7a/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Android/armeabi-v7a)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/arm64-v8a/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Android/arm64-v8a)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/x86/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Android/x86)
    endif()

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/SteamAudioFMODStudio)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin DESTINATION unreal/SteamAudioFMODStudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/Source DESTINATION unreal/SteamAudioFMODStudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/Resources DESTINATION unreal/SteamAudioFMODStudio)
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_EDITOR}-Win64-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win64-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win64-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_WIN32)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win32-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win32-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Linux-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Linux-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
elseif (IPL_OS_MACOS)
    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/SteamAudio)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/SteamAudio.uplugin DESTINATION unreal/SteamAudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/Source DESTINATION unreal/SteamAudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/Content DESTINATION unreal/SteamAudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio/Resources DESTINATION unreal/SteamAudio)
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_EDITOR}-Mac-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Mac-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Mac-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-iOS-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-iOS-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/FMODStudio)")
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Mac/phonon_fmod.bundle DESTINATION unreal/FMODStudio/Binaries/Mac)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/IOS/libphonon_fmod.a DESTINATION unreal/FMODStudio/Binaries/IOS)
    endif()

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/SteamAudioFMODStudio)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin DESTINATION unreal/SteamAudioFMODStudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/Source DESTINATION unreal/SteamAudioFMODStudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/Content DESTINATION unreal/SteamAudioFMODStudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/Resources DESTINATION unreal/SteamAudioFMODStudio)
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_EDITOR}-Mac-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Mac-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Mac-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-iOS-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-iOS-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
endif()
