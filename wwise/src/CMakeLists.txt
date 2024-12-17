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

# ---------------------------------------------------------------------------------------------------------------------
# DEPENDENCIES
# ---------------------------------------------------------------------------------------------------------------------

find_package(Python3 REQUIRED)
find_package(Wwise REQUIRED)
find_package(SteamAudio REQUIRED)


# ---------------------------------------------------------------------------------------------------------------------
# CONFIGURE
# ---------------------------------------------------------------------------------------------------------------------

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SoundEnginePlugin/SteamAudioVersion.h.in ${CMAKE_CURRENT_SOURCE_DIR}/SoundEnginePlugin/SteamAudioVersion.h)

if (IPL_OS_WINDOWS)
    execute_process(
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} premake Windows_vc160
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    execute_process(
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} premake Windows_vc170
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if (IPL_CPU_X64)
        execute_process(
            COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} premake Authoring
            WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
elseif (IPL_OS_LINUX)
    execute_process(
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} premake Linux
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )
elseif (IPL_OS_MACOS)
    execute_process(
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} premake Mac
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )
elseif (IPL_OS_ANDROID)
    execute_process(
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} premake Android
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )
elseif (IPL_OS_IOS)
    execute_process(
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} premake iOS
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

if (IPL_OS_MACOS OR IPL_OS_IOS)
    set(Wwise_XCODE_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/tmpToolchainVers.txt)
    set(Wwise_XCODE_WRITTEN FALSE)

    if (DEFINED ENV{AK_XCODE_DEVELOPER_DIR_1400})
        file(WRITE ${Wwise_XCODE_FILENAME} "Xcode1400\n")
        set(Wwise_XCODE_WRITTEN TRUE)
    endif()

    if (DEFINED ENV{AK_XCODE_DEVELOPER_DIR_1500})
        if (Wwise_XCODE_WRITTEN)
            file(APPEND ${Wwise_XCODE_FILENAME} "Xcode1500\n")
        else()
            file(WRITE ${Wwise_XCODE_FILENAME} "Xcode1500\n")
        endif()
    endif()
endif()


# ---------------------------------------------------------------------------------------------------------------------
# BUILD
# ---------------------------------------------------------------------------------------------------------------------

add_custom_target(phonon_wwise ALL)

if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE Win32)
    elseif (IPL_CPU_X64)
        set(Wwise_ARCHITECTURE x64)
    endif()

    add_custom_target(phonon_wwise_windows_vs2019
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Windows_vc160 -x ${Wwise_ARCHITECTURE} -t vc160 -c Debug
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Windows_vc160 -x ${Wwise_ARCHITECTURE} -t vc160 -c Profile
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Windows_vc160 -x ${Wwise_ARCHITECTURE} -t vc160 -c Release
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_custom_target(phonon_wwise_windows_vs2022
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Windows_vc170 -x ${Wwise_ARCHITECTURE} -t vc170 -c Debug
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Windows_vc170 -x ${Wwise_ARCHITECTURE} -t vc170 -c Profile
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Windows_vc170 -x ${Wwise_ARCHITECTURE} -t vc170 -c Release
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_dependencies(phonon_wwise phonon_wwise_windows_vs2019)
    add_dependencies(phonon_wwise phonon_wwise_windows_vs2022)

    if (IPL_CPU_X64)
        add_custom_target(phonon_wwise_windows_authoring
            COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Authoring -x x64 -t vc170 -c Debug
            COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Authoring -x x64 -t vc170 -c Release
            WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
        )

        add_custom_target(phonon_wwise_documentation
            COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Documentation
            WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
        )

        add_dependencies(phonon_wwise phonon_wwise_windows_authoring phonon_wwise_documentation)
    endif()
elseif (IPL_OS_LINUX AND IPL_CPU_X64)
    add_custom_target(phonon_wwise_linux
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Linux -x x64 -c Debug
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Linux -x x64 -c Profile
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Linux -x x64 -c Release
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_dependencies(phonon_wwise phonon_wwise_linux)
elseif (IPL_OS_MACOS)
    add_custom_target(phonon_wwise_osx
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Mac --toolchain-vers ${CMAKE_CURRENT_BINARY_DIR}/tmpToolchainVers.txt -c Debug
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Mac --toolchain-vers ${CMAKE_CURRENT_BINARY_DIR}/tmpToolchainVers.txt -c Profile
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Mac --toolchain-vers ${CMAKE_CURRENT_BINARY_DIR}/tmpToolchainVers.txt -c Release
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_dependencies(phonon_wwise phonon_wwise_osx)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(Wwise_ARCHITECTURE armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(Wwise_ARCHITECTURE arm64-v8a)
    elseif (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE x86)
    elseif (IPL_CPU_X64)
        set(Wwise_ARCHITECTURE x86_64)
    endif()

    add_custom_target(phonon_wwise_android
        COMMAND             ${CMAKE_COMMAND} -E env NDKROOT=${CMAKE_ANDROID_NDK} ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Android -x ${Wwise_ARCHITECTURE} -c Debug
        COMMAND             ${CMAKE_COMMAND} -E env NDKROOT=${CMAKE_ANDROID_NDK} ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Android -x ${Wwise_ARCHITECTURE} -c Profile
        COMMAND             ${CMAKE_COMMAND} -E env NDKROOT=${CMAKE_ANDROID_NDK} ${Python3_EXECUTABLE} ${Wwise_WP_PY} build Android -x ${Wwise_ARCHITECTURE} -c Release
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_dependencies(phonon_wwise phonon_wwise_android)
elseif (IPL_OS_IOS)
    add_custom_target(phonon_wwise_ios
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build iOS --toolchain-vers ${CMAKE_CURRENT_BINARY_DIR}/tmpToolchainVers.txt -c Debug
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build iOS --toolchain-vers ${CMAKE_CURRENT_BINARY_DIR}/tmpToolchainVers.txt -c Profile
        COMMAND             ${Python3_EXECUTABLE} ${Wwise_WP_PY} build iOS --toolchain-vers ${CMAKE_CURRENT_BINARY_DIR}/tmpToolchainVers.txt -c Release
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_dependencies(phonon_wwise phonon_wwise_ios)
endif()


# ---------------------------------------------------------------------------------------------------------------------
# POST-BUILD: wwise -> unity
# ---------------------------------------------------------------------------------------------------------------------

set(UNITY_WWISE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Wwise/API/Runtime/Plugins)

add_custom_target(phonon_wwise_copy_unity ALL)

if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE Win32)
        set(Wwise_ARCHITECTURE_UNITY x86)
    elseif (IPL_CPU_X64)
        set(Wwise_ARCHITECTURE x64)
        set(Wwise_ARCHITECTURE_UNITY x86_64)
    endif()

    # copy the vs2022 release build
    set(Wwise_TOOLCHAIN vc170)
    set(Wwise_CONFIGURATION Release)

    add_custom_target(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} ALL
        COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNITY_WWISE_PLUGIN_DIR}/Windows/${Wwise_ARCHITECTURE_UNITY}/DSP"
        COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin/SteamAudioWwise.dll" "${UNITY_WWISE_PLUGIN_DIR}/Windows/${Wwise_ARCHITECTURE_UNITY}/DSP"
        COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin/SteamAudioWwise.pdb" "${UNITY_WWISE_PLUGIN_DIR}/Windows/${Wwise_ARCHITECTURE_UNITY}/DSP"
        VERBATIM
    )

    add_dependencies(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} phonon_wwise_copy_unity)
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X64)
        set(Wwise_ARCHITECTURE x86_64)
    endif()

    # copy the release build
    set(Wwise_CONFIGURATION Release)

    add_custom_target(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} ALL
        COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNITY_WWISE_PLUGIN_DIR}/Linux/${Wwise_ARCHITECTURE}/DSP"
        COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Linux_x64/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.so" "${UNITY_WWISE_PLUGIN_DIR}/Linux/${Wwise_ARCHITECTURE}/DSP"
        VERBATIM
    )

    add_dependencies(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} phonon_wwise_copy_unity)
elseif (IPL_OS_MACOS)
    # copy the xcode15 release build
    set(Wwise_ARCHITECTURE universal)
    set(Wwise_TOOLCHAIN Xcode1500)
    set(Wwise_CONFIGURATION Release)

    add_custom_target(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} ALL
        COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNITY_WWISE_PLUGIN_DIR}/Mac/DSP"
        COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.dylib" "${UNITY_WWISE_PLUGIN_DIR}/Mac/DSP/libSteamAudioWwise.bundle"
        VERBATIM
    )

    add_dependencies(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} phonon_wwise_copy_unity)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(Wwise_ARCHITECTURE armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(Wwise_ARCHITECTURE arm64-v8a)
    elseif (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE x86)
    endif()

    # copy the release build
    set(Wwise_CONFIGURATION Release)

    add_custom_target(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} ALL
        COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNITY_WWISE_PLUGIN_DIR}/Android/${Wwise_ARCHITECTURE}/DSP"
        COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.so" "${UNITY_WWISE_PLUGIN_DIR}/Android/${Wwise_ARCHITECTURE}/DSP"
        VERBATIM
    )

    add_dependencies(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE} phonon_wwise_copy_unity)
elseif (IPL_OS_IOS)
    # copy the xcode15 release build
    set(Wwise_ARCHITECTURE arm64)
    set(Wwise_TOOLCHAIN Xcode1500)
    set(Wwise_CONFIGURATION Release)

    foreach (Wwise_DESTINATION IN ITEMS iphoneos iphonesimulator)
        add_custom_target(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE}_${Wwise_DESTINATION} ALL
            COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNITY_WWISE_PLUGIN_DIR}/iOS/${Wwise_DESTINATION}/DSP"
            COMMAND     ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/SoundEnginePlugin/SteamAudioWwiseFXFactory.h" "${UNITY_WWISE_PLUGIN_DIR}/iOS/${Wwise_DESTINATION}/DSP"
            COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/lib/libSteamAudioWwiseFX.a" "${UNITY_WWISE_PLUGIN_DIR}/iOS/${Wwise_DESTINATION}/DSP"
            VERBATIM
        )

        add_dependencies(phonon_wwise_copy_unity_${Wwise_ARCHITECTURE}_${Wwise_DESTINATION} phonon_wwise_copy_unity)
    endforeach()
endif()

add_dependencies(phonon_wwise_copy_unity phonon_wwise)


# ---------------------------------------------------------------------------------------------------------------------
# POST-BUILD: wwise -> unreal
# ---------------------------------------------------------------------------------------------------------------------

set(UNREAL_WWISE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/Wwise/ThirdParty)

add_custom_target(phonon_wwise_copy_unreal ALL)

if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE Win32)
    elseif (IPL_CPU_X64)
        set(Wwise_ARCHITECTURE x64)
    endif()

    foreach (Wwise_TOOLCHAIN IN ITEMS vc160 vc170)
        foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
            add_custom_target(phonon_wwise_copy_unreal_${Wwise_TOOLCHAIN}_${Wwise_CONFIGURATION}
                COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin"
                COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib"
                COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}(StaticCRT)/lib"
                COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin/SteamAudioWwise.dll" "${UNREAL_WWISE_PLUGIN_DIR}/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin"
                COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib/SteamAudioWwiseFX.lib" "${UNREAL_WWISE_PLUGIN_DIR}/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib"
                COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}(StaticCRT)/lib/SteamAudioWwiseFX.lib" "${UNREAL_WWISE_PLUGIN_DIR}/${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}(StaticCRT)/lib"
                VERBATIM
            )

            add_dependencies(phonon_wwise_copy_unreal phonon_wwise_copy_unreal_${Wwise_TOOLCHAIN}_${Wwise_CONFIGURATION})
        endforeach()
    endforeach()
elseif (IPL_OS_LINUX)
    foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
        add_custom_target(phonon_wwise_copy_unreal_${Wwise_CONFIGURATION}
            COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/Linux_x64/${Wwise_CONFIGURATION}/bin"
            COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/Linux_x64/${Wwise_CONFIGURATION}/lib"
            COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Linux_x64/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.so" "${UNREAL_WWISE_PLUGIN_DIR}/Linux_x64/${Wwise_CONFIGURATION}/bin"
            COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Linux_x64/${Wwise_CONFIGURATION}/lib/libSteamAudioWwiseFX.a" "${UNREAL_WWISE_PLUGIN_DIR}/Linux_x64/${Wwise_CONFIGURATION}/lib"
            VERBATIM
        )

        add_dependencies(phonon_wwise_copy_unreal phonon_wwise_copy_unreal_${Wwise_CONFIGURATION})
    endforeach()
elseif (IPL_OS_MACOS)
    foreach (Wwise_TOOLCHAIN IN ITEMS Xcode1400 Xcode1500)
        foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
            add_custom_target(phonon_wwise_copy_unreal_${Wwise_TOOLCHAIN}_${Wwise_CONFIGURATION}
                COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin"
                COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib"
                COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.dylib" "${UNREAL_WWISE_PLUGIN_DIR}/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/bin"
                COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib/libSteamAudioWwiseFX.a" "${UNREAL_WWISE_PLUGIN_DIR}/Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}/lib"
                VERBATIM
            )

            add_dependencies(phonon_wwise_copy_unreal phonon_wwise_copy_unreal_${Wwise_TOOLCHAIN}_${Wwise_CONFIGURATION})
        endforeach()
    endforeach()
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(Wwise_ARCHITECTURE armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(Wwise_ARCHITECTURE arm64-v8a)
    elseif (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE x86)
    elseif (IPL_CPU_X64)
        set(Wwise_ARCHITECTURE x86_64)
    endif()

    foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
        add_custom_target(phonon_wwise_copy_unreal_${Wwise_CONFIGURATION}
            COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/bin"
            COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/lib"
            COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/bin/libSteamAudioWwise.so" "${UNREAL_WWISE_PLUGIN_DIR}/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/bin"
            COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/lib/libSteamAudioWwiseFX.a" "${UNREAL_WWISE_PLUGIN_DIR}/Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION}/lib"
            VERBATIM
        )

        add_dependencies(phonon_wwise_copy_unreal phonon_wwise_copy_unreal_${Wwise_CONFIGURATION})
    endforeach()
elseif (IPL_OS_IOS)
    foreach (Wwise_TOOLCHAIN IN ITEMS Xcode1400 Xcode1500)
        foreach (Wwise_CONFIGURATION IN ITEMS Debug Profile Release)
            foreach (Wwise_DESTINATION IN ITEMS iphoneos iphonesimulator)
                add_custom_target(phonon_wwise_copy_unreal_${Wwise_TOOLCHAIN}_${Wwise_CONFIGURATION}_${Wwise_DESTINATION}
                    COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/bin"
                    COMMAND     ${CMAKE_COMMAND} -E make_directory "${UNREAL_WWISE_PLUGIN_DIR}/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/lib"
                    COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/bin/libSteamAudioWwise.dylib" "${UNREAL_WWISE_PLUGIN_DIR}/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/bin"
                    COMMAND     ${CMAKE_COMMAND} -E copy "${Wwise_INCLUDE_DIR}/../../iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/lib/libSteamAudioWwiseFX.a" "${UNREAL_WWISE_PLUGIN_DIR}/iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION}/lib"
                    VERBATIM
                )

                add_dependencies(phonon_wwise_copy_unreal phonon_wwise_copy_unreal_${Wwise_TOOLCHAIN}_${Wwise_CONFIGURATION}_${Wwise_DESTINATION})
            endforeach()
        endforeach()
    endforeach()
endif()

add_dependencies(phonon_wwise_copy_unreal phonon_wwise)


# ---------------------------------------------------------------------------------------------------------------------
# INSTALL
# ---------------------------------------------------------------------------------------------------------------------

set(Wwise_CONFIGURATIONS "Debug" "Profile" "Release")
set(Wwise_AUTHORING_CONFIGURATIONS "Debug" "Release")

if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE Win32)
    else()
        set(Wwise_ARCHITECTURE x64)
    endif()

    set(Wwise_TOOLCHAINS "vc160" "vc170")

    foreach (Wwise_TOOLCHAIN IN ITEMS ${Wwise_TOOLCHAINS})
        foreach (Wwise_CONFIGURATION IN ITEMS ${Wwise_CONFIGURATIONS})
            set(Wwise_INSTALL_SUBDIR ${Wwise_ARCHITECTURE}_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION})
            set(Wwise_INSTALL_SRCDIR ${Wwise_INCLUDE_DIR}/../../${Wwise_INSTALL_SUBDIR})

            install(
                FILES 
                    ${Wwise_INSTALL_SRCDIR}/lib/SteamAudioWwiseFX.lib
                    ${Wwise_INSTALL_SRCDIR}/lib/SteamAudioWwiseFX.pdb
                DESTINATION 
                    wwise/SDK/${Wwise_INSTALL_SUBDIR}/lib
            )

            install(
                FILES 
                    ${Wwise_INSTALL_SRCDIR}/bin/SteamAudioWwise.dll
                    ${Wwise_INSTALL_SRCDIR}/bin/SteamAudioWwise.pdb
                DESTINATION 
                    wwise/SDK/${Wwise_INSTALL_SUBDIR}/bin
            )

            install(
                FILES 
                    "${Wwise_INSTALL_SRCDIR}(StaticCRT)/lib/SteamAudioWwiseFX.lib"
                    "${Wwise_INSTALL_SRCDIR}(StaticCRT)/lib/SteamAudioWwiseFX.pdb"
                DESTINATION 
                    "wwise/SDK/${Wwise_INSTALL_SUBDIR}(StaticCRT)/lib"
            )
        endforeach()
    endforeach()

    if (IPL_CPU_X64)
        install(
            FILES
                ${Wwise_INCLUDE_DIR}/Plugin/SteamAudioWwiseFXFactory.h
            DESTINATION
                wwise/SDK/include/AK/Plugin
        )

        foreach (Wwise_CONFIGURATION IN ITEMS ${Wwise_AUTHORING_CONFIGURATIONS})
            install(
                FILES
                    ${Wwise_INCLUDE_DIR}/../../../Authoring/x64/${Wwise_CONFIGURATION}/bin/Plugins/SteamAudioWwise.dll
                    ${Wwise_INCLUDE_DIR}/../../../Authoring/x64/${Wwise_CONFIGURATION}/bin/Plugins/SteamAudioWwise.pdb
                    ${Wwise_INCLUDE_DIR}/../../../Authoring/x64/${Wwise_CONFIGURATION}/bin/Plugins/SteamAudioWwise.xml
                DESTINATION
                    wwise/Authoring/x64/${Wwise_CONFIGURATION}/bin/Plugins
            )
        endforeach()

        install(
            DIRECTORY
                ${Wwise_INCLUDE_DIR}/../../../Authoring/Data/Plugins/SteamAudioWwise
            DESTINATION
                wwise/Authoring/Data/Plugins
        )
    endif()

elseif (IPL_OS_LINUX AND IPL_CPU_X64)
    foreach (Wwise_CONFIGURATION IN ITEMS ${Wwise_CONFIGURATIONS})
        set(Wwise_INSTALL_SUBDIR Linux_x64/${Wwise_CONFIGURATION})
        set(Wwise_INSTALL_SRCDIR ${Wwise_INCLUDE_DIR}/../../${Wwise_INSTALL_SUBDIR})

        install(
            FILES 
                ${Wwise_INSTALL_SRCDIR}/lib/libSteamAudioWwiseFX.a
            DESTINATION 
                wwise/SDK/${Wwise_INSTALL_SUBDIR}/lib
        )

        install(
            FILES 
                ${Wwise_INSTALL_SRCDIR}/bin/libSteamAudioWwise.so
            DESTINATION 
                wwise/SDK/${Wwise_INSTALL_SUBDIR}/bin
        )
    endforeach()

elseif (IPL_OS_MACOS)
    set(Wwise_TOOLCHAINS "Xcode1400" "Xcode1500")

    foreach (Wwise_TOOLCHAIN IN ITEMS ${Wwise_TOOLCHAINS})
        foreach (Wwise_CONFIGURATION IN ITEMS ${Wwise_CONFIGURATIONS})
            set(Wwise_INSTALL_SUBDIR Mac_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION})
            set(Wwise_INSTALL_SRCDIR ${Wwise_INCLUDE_DIR}/../../${Wwise_INSTALL_SUBDIR})

            install(
                FILES 
                    ${Wwise_INSTALL_SRCDIR}/lib/libSteamAudioWwiseFX.a
                DESTINATION 
                    wwise/SDK/${Wwise_INSTALL_SUBDIR}/lib
            )

            install(
                FILES 
                    ${Wwise_INSTALL_SRCDIR}/bin/libSteamAudioWwise.dylib
                DESTINATION 
                    wwise/SDK/${Wwise_INSTALL_SUBDIR}/bin
            )

            install(
                DIRECTORY 
                    ${Wwise_INSTALL_SRCDIR}/bin/libSteamAudioWwise.dylib.dSYM
                DESTINATION 
                    wwise/SDK/${Wwise_INSTALL_SUBDIR}/bin
            )
        endforeach()
    endforeach()

elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(Wwise_ARCHITECTURE armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(Wwise_ARCHITECTURE arm64-v8a)
    elseif (IPL_CPU_X86)
        set(Wwise_ARCHITECTURE x86)
    elseif (IPL_CPU_X64)
        set(Wwise_ARCHITECTURE x86_64)
    endif()

    foreach (Wwise_CONFIGURATION IN ITEMS ${Wwise_CONFIGURATIONS})
        set(Wwise_INSTALL_SUBDIR Android_${Wwise_ARCHITECTURE}/${Wwise_CONFIGURATION})
        set(Wwise_INSTALL_SRCDIR ${Wwise_INCLUDE_DIR}/../../${Wwise_INSTALL_SUBDIR})

        install(
            FILES 
                ${Wwise_INSTALL_SRCDIR}/lib/libSteamAudioWwiseFX.a
                ${Wwise_INSTALL_SRCDIR}/lib/libSteamAudioWwise.so
            DESTINATION 
                wwise/SDK/${Wwise_INSTALL_SUBDIR}/lib
        )

        install(
            FILES 
                ${Wwise_INSTALL_SRCDIR}/bin/libSteamAudioWwise.so
            DESTINATION 
                wwise/SDK/${Wwise_INSTALL_SUBDIR}/bin
        )
    endforeach()

elseif (IPL_OS_IOS)
    set(Wwise_TOOLCHAINS "Xcode1400" "Xcode1500")
    set(Wwise_DESTINATIONS "iphoneos" "iphonesimulator")

    foreach (Wwise_TOOLCHAIN IN ITEMS ${Wwise_TOOLCHAINS})
        foreach (Wwise_CONFIGURATION IN ITEMS ${Wwise_CONFIGURATIONS})
            foreach (Wwise_DESTINATION IN ITEMS ${Wwise_DESTINATIONS})
                set(Wwise_INSTALL_SUBDIR iOS_${Wwise_TOOLCHAIN}/${Wwise_CONFIGURATION}-${Wwise_DESTINATION})
                set(Wwise_INSTALL_SRCDIR ${Wwise_INCLUDE_DIR}/../../${Wwise_INSTALL_SUBDIR})

                install(
                    FILES 
                        ${Wwise_INSTALL_SRCDIR}/lib/libSteamAudioWwiseFX.a
                    DESTINATION 
                        wwise/SDK/${Wwise_INSTALL_SUBDIR}/lib
                )
    
                install(
                    FILES 
                        ${Wwise_INSTALL_SRCDIR}/bin/libSteamAudioWwise.dylib
                    DESTINATION 
                        wwise/SDK/${Wwise_INSTALL_SUBDIR}/bin
                )
    
                install(
                    DIRECTORY 
                        ${Wwise_INSTALL_SRCDIR}/bin/libSteamAudioWwise.dylib.dSYM
                    DESTINATION 
                        wwise/SDK/${Wwise_INSTALL_SUBDIR}/bin
                )
            endforeach()
        endforeach()
    endforeach()

endif()


# ---------------------------------------------------------------------------------------------------------------------
# PACKAGING
# ---------------------------------------------------------------------------------------------------------------------

set(PACKAGE_SCRIPT ${CMAKE_HOME_DIRECTORY}/build/package_wwise.cmake)

file(WRITE  ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_directory ${CMAKE_HOME_DIRECTORY}/bin/wwise ${CMAKE_HOME_DIRECTORY}/include/wwise COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Common --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Windows_vc160 --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Windows_vc170 --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Linux --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Mac --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Android --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package iOS --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Authoring --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} package Documentation --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND ${Python3_EXECUTABLE} ${Wwise_WP_PY} generate-bundle --version=2024.${CMAKE_PROJECT_VERSION} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E make_directory ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Common.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Windows_vc160.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Windows_vc170.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Linux.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Mac.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Android.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.iOS.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_Authoring_Windows_Debug.x64.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_Authoring_Windows_Release.x64.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_Authoring.Documentation.tar.xz ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy ${CMAKE_CURRENT_SOURCE_DIR}/bundle.json ${CMAKE_HOME_DIRECTORY}/bin/wwise_launcher WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Common.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Windows_vc160.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Windows_vc170.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Linux.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Mac.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.Android.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_SDK.iOS.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_Authoring_Windows_Debug.x64.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_Authoring_Windows_Release.x64.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/SteamAudioWwise_v2024.${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}_Build${CMAKE_PROJECT_VERSION_PATCH}_Authoring.Documentation.tar.xz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
file(APPEND ${PACKAGE_SCRIPT} "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm ${CMAKE_CURRENT_SOURCE_DIR}/bundle.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND_ECHO STDOUT)\n")
