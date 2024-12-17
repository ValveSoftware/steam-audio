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
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin.in ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin)

#
# UNREAL PLUGIN
#

set(UBT_ARGS "-iwyu -noubtmakefiles -nohotreload -forceunity")

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
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Development.xml ${UBT_ARGS} -architectures=arm64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProject/HostProject.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins/SteamAudio/SteamAudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Shipping.xml ${UBT_ARGS} -architectures=arm64
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

add_dependencies(phonon_unreal_win64 phonon_unreal_setup)
add_dependencies(phonon_unreal_linux phonon_unreal_setup)
add_dependencies(phonon_unreal_mac phonon_unreal_setup)
add_dependencies(phonon_unreal_android phonon_unreal_setup)
add_dependencies(phonon_unreal_ios phonon_unreal_setup)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    add_dependencies(phonon_unreal phonon_unreal_win64)
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
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Development.xml ${UBT_ARGS} -architectures=arm64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/HostProjectFMOD.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Shipping.xml ${UBT_ARGS} -architectures=arm64
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

add_dependencies(phonon_unreal_fmod_win64 phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_linux phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_mac phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_android phonon_unreal_fmod_setup)
add_dependencies(phonon_unreal_fmod_ios phonon_unreal_fmod_setup)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    add_dependencies(phonon_unreal_fmod phonon_unreal_fmod_win64)
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
# UNREAL + WWISE PLUGIN
#

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise.uproject "{ \"FileVersion\": 3, \"Plugins\": [ { \"Name\": \"SteamAudio\", \"Enabled\": true }, { \"Name\": \"Wwise\", \"Enabled\": true }, { \"Name\": \"SteamAudioWwise\", \"Enabled\": true } ] }")

add_custom_target(phonon_unreal_wwise_setup
    COMMAND     cmake -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise
    COMMAND     cmake -E copy ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise.uproject ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudio
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/Wwise
    COMMAND     cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise
    COMMAND     cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudio ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudio
    COMMAND     cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/Wwise
    COMMAND     cmake -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise
    COMMAND     cmake -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/Intermediate
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/Wwise/Binaries/Win64/${UE_TARGET_EDITOR}.modules
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/Wwise/Binaries/Win64/${UE_TARGET_EDITOR}-Wwise.dll
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/Wwise/Binaries/Win64/${UE_TARGET_EDITOR}-Wwise.pdb
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/Wwise/Binaries/Win64/${UE_TARGET_EDITOR}-WwiseEditor.dll
    COMMAND     cmake -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/Wwise/Binaries/Win64/${UE_TARGET_EDITOR}-WwiseEditor.pdb
    VERBATIM
)

add_custom_target(phonon_unreal_wwise_win64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_EDITOR} Win64 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_EDITOR}-Win64-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win64 Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Win64-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Win64 Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Win64-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_wwise_linux
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Linux Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Linux-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Linux Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Linux-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_wwise_mac
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_EDITOR} Mac Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_EDITOR}-Mac-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Mac Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Mac-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Mac Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Mac-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_wwise_android
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Android-Development.xml ${UBT_ARGS} -architectures=arm64
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} Android Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Android-Shipping.xml ${UBT_ARGS} -architectures=arm64
    VERBATIM
)

add_custom_target(phonon_unreal_wwise_ios
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} iOS Development -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-iOS-Development.xml ${UBT_ARGS}
    COMMAND     ${Unreal_EXECUTABLE} ${UE_TARGET_GAME} iOS Shipping -project=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/HostProjectWwise.uproject -plugin=${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin -manifest=${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-iOS-Shipping.xml ${UBT_ARGS}
    VERBATIM
)

add_custom_target(phonon_unreal_wwise ALL
    COMMAND     cmake -E echo Built plugin SteamAudioWwise.
    VERBATIM
)

add_dependencies(phonon_unreal_wwise_win64 phonon_unreal_wwise_setup)
add_dependencies(phonon_unreal_wwise_linux phonon_unreal_wwise_setup)
add_dependencies(phonon_unreal_wwise_mac phonon_unreal_wwise_setup)
add_dependencies(phonon_unreal_wwise_android phonon_unreal_wwise_setup)
add_dependencies(phonon_unreal_wwise_ios phonon_unreal_wwise_setup)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    add_dependencies(phonon_unreal_wwise phonon_unreal_wwise_win64)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        add_dependencies(phonon_unreal_wwise phonon_unreal_wwise_linux)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        add_dependencies(phonon_unreal_wwise phonon_unreal_wwise_android)
    endif()
elseif (IPL_OS_MACOS)
    add_dependencies(phonon_unreal_wwise phonon_unreal_wwise_mac)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS)
        add_definitions(phonon_unreal_wwise phonon_unreal_wwise_ios)
    endif()
endif()

add_dependencies(phonon_unreal_wwise phonon_unreal)


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
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Linux-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Linux-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProject/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudio-${UE_TARGET_GAME}-Android-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/SteamAudioFMODStudio)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/SteamAudioFMODStudio.uplugin DESTINATION unreal/SteamAudioFMODStudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/Source DESTINATION unreal/SteamAudioFMODStudio)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioFMODStudio/Resources DESTINATION unreal/SteamAudioFMODStudio)
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_EDITOR}-Win64-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win64-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Win64-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Linux-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Linux-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectFMOD/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioFMODStudio-${UE_TARGET_GAME}-Android-Shipping.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endif()

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/SteamAudioWwise)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin DESTINATION unreal/SteamAudioWwise)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/Source DESTINATION unreal/SteamAudioWwise)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/Resources DESTINATION unreal/SteamAudioWwise)
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_EDITOR}-Win64-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    foreach (Unreal_CONFIGURATION IN ITEMS Development Shipping)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Win64-${Unreal_CONFIGURATION}.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endforeach()
    if (STEAMAUDIO_CROSSCOMPILE_LINUX)
        foreach (Unreal_CONFIGURATION IN ITEMS Development Shipping)
            install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Linux-${Unreal_CONFIGURATION}.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        endforeach()
    endif()
    if (STEAMAUDIO_CROSSCOMPILE_ANDROID)
        foreach (Unreal_CONFIGURATION IN ITEMS Development Shipping)
            install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Android-${Unreal_CONFIGURATION}.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        endforeach()
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

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/SteamAudioWwise)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/SteamAudioWwise.uplugin DESTINATION unreal/SteamAudioWwise)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/Source DESTINATION unreal/SteamAudioWwise)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/SteamAudioWwise/Resources DESTINATION unreal/SteamAudioWwise)
    install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_EDITOR}-Mac-Development.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    foreach (Unreal_CONFIGURATION IN ITEMS Development Shipping)
        install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-Mac-${Unreal_CONFIGURATION}.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
    endforeach()
    if (STEAMAUDIO_CROSSCOMPILE_IOS)
        foreach (Unreal_CONFIGURATION IN ITEMS Development Shipping)
            install(CODE "execute_process(COMMAND python copy_files_from_manifest.py -s ${CMAKE_CURRENT_BINARY_DIR}/HostProjectWwise/Plugins -d ${CMAKE_HOME_DIRECTORY}/bin/unreal -m ${CMAKE_CURRENT_BINARY_DIR}/Manifest-SteamAudioWwise-${UE_TARGET_GAME}-iOS-${Unreal_CONFIGURATION}.xml WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build)")
        endforeach()
    endif()
endif()

if ((IPL_OS_WINDOWS AND IPL_CPU_X64) OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/FMODStudio)")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Win64/phonon_fmod.dll DESTINATION unreal/FMODStudio/Binaries/Win64)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Linux/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Linux)
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/armeabi-v7a/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Android/armeabi-v7a)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/arm64-v8a/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Android/arm64-v8a)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Android/x86/libphonon_fmod.so DESTINATION unreal/FMODStudio/Binaries/Android/x86)
    endif()

    install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/Wwise)")
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/include/AK/Plugin/SteamAudioWwiseFXFactory.h"
        DESTINATION
            "unreal/Wwise/ThirdParty/include/AK/Plugin"
    )
    foreach (Wwise_ARCHITECTURE IN ITEMS Win32 x64)
        foreach (Wwise_TOOLCHAIN IN ITEMS vc160 vc170)
            foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
                install(
                    FILES 
                        "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib/SteamAudioWwiseFX.lib"
                    DESTINATION
                        "unreal/Wwise/ThirdParty/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib"
                )

                install(
                    FILES 
                        "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin/SteamAudioWwise.dll"
                    DESTINATION
                        "unreal/Wwise/ThirdParty/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin"
                )

                install(
                    FILES 
                        "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}(StaticCRT)/lib/SteamAudioWwiseFX.lib"
                    DESTINATION
                        "unreal/Wwise/ThirdParty/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}(StaticCRT)/lib"
                )
            endforeach()
        endforeach()
    endforeach()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_LINUX OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
            install(
                FILES 
                    "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/Linux_x64/${Wwise_CONFIGURATION}/lib/libSteamAudioWwiseFX.a"
                DESTINATION
                    "unreal/Wwise/ThirdParty/Linux_x64/${Wwise_CONFIGURATION}/lib"
            )

            install(
                FILES 
                    "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/Linux_x64/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.so"
                DESTINATION
                    "unreal/Wwise/ThirdParty/Linux_x64/${Wwise_CONFIGURATION}/bin"
            )
        endforeach()
    endif()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_ANDROID OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        foreach (Wwise_ARCHITECTURE IN ITEMS armeabi-v7a arm64-v8a x86 x86_64)
            foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
                install(
                    FILES 
                        "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/lib/libSteamAudioWwiseFX.a"
                    DESTINATION
                        "unreal/Wwise/ThirdParty/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/lib"
                )

                install(
                    FILES 
                        "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.so"
                    DESTINATION
                        "unreal/Wwise/ThirdParty/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/bin"
                )
            endforeach()
        endforeach()
    endif()
endif()

if (IPL_OS_MACOS OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
    if (NOT STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/FMODStudio)")
    endif()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/Mac/libphonon_fmod.dylib DESTINATION unreal/FMODStudio/Binaries/Mac)
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/FMODStudio/Binaries/IOS/libphonon_fmod.a DESTINATION unreal/FMODStudio/Binaries/IOS)
    endif()

    if (NOT STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        install(CODE "execute_process(COMMAND cmake -E rm -rf ${CMAKE_HOME_DIRECTORY}/bin/unreal/Wwise)")
    endif()
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/include/AK/Plugin/SteamAudioWwiseFXFactory.h"
        DESTINATION
            "unreal/Wwise/ThirdParty/include/AK/Plugin"
    )
    foreach (Wwise_TOOLCHAIN IN ITEMS Xcode1400 Xcode1500)
        foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
            install(
                FILES 
                    "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib/libSteamAudioWwiseFX.a"
                DESTINATION
                    "unreal/Wwise/ThirdParty/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib"
            )

            install(
                FILES 
                    "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.dylib"
                DESTINATION
                    "unreal/Wwise/ThirdParty/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin"
            )
        endforeach()
    endforeach()
    if (STEAMAUDIOUNREAL_CROSSCOMPILE_IOS OR STEAMAUDIOUNREAL_INSTALL_ALL_PLATFORMS)
        foreach (Wwise_TOOLCHAIN IN ITEMS Xcode1400 Xcode1500)
            foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
                foreach (Wwise_DESTINATION IN ITEMS iphoneos iphonesimulator)
                    install(
                        FILES 
                            "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/lib/libSteamAudioWwiseFX.a"
                        DESTINATION
                            "unreal/Wwise/ThirdParty/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/lib"
                    )

                    install(
                        FILES 
                            "${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioUnreal/Plugins/Wwise/ThirdParty/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/bin/libSteamAudioWwise.dylib"
                        DESTINATION
                            "unreal/Wwise/ThirdParty/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/bin"
                    )
                endforeach()
            endforeach()
        endforeach()
    endif()
endif()
