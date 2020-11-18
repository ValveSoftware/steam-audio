# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

###############################################################################
# GLOBAL SETTINGS
###############################################################################

cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 14)
project(Phonon VERSION 2.0.19)

set(CMAKE_MODULE_PATH "${CMAKE_HOME_DIRECTORY}/build")
include(cotire)

include_directories(include include/phonon)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BUILDING_64BIT TRUE)
else()
  set(BUILDING_64BIT FALSE)
endif()

if (UNIX AND NOT APPLE AND NOT ANDROID)
  set(LINUX TRUE)
else()
  set(LINUX FALSE)
endif()

if (MSVC OR XCODE)
    set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}-debug)
    set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}-debug)
    set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}-debug)
    set (CMAKE_PDB_OUTPUT_DIRECTORY_DEBUG             ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}-debug)
    set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}-release)
    set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}-release)
    set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}-release)
    set (CMAKE_PDB_OUTPUT_DIRECTORY_RELEASE           ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}-release)
endif ()

if (MSVC)
    set (CMAKE_CONFIGURATION_TYPES                      "Debug;Development;Release")
    set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEVELOPMENT     ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}-development)
    set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEVELOPMENT     ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}-development)
    set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEVELOPMENT     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}-development)
    set (CMAKE_PDB_OUTPUT_DIRECTORY_DEVELOPMENT         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}-development)
    set_property (GLOBAL PROPERTY DEBUG_CONFIGURATIONS  "Debug;Development")
endif()


###############################################################################
# WINDOWS-SPECIFIC SETTINGS
###############################################################################

if (MSVC)
  add_definitions(                          -DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
  add_compile_options(                      -EHsc -MP -W3 -Zi)
  set(CMAKE_CXX_FLAGS_DEBUG                 "/D_DEBUG /Od /RTC1 /MTd")
  set(CMAKE_CXX_FLAGS_DEVELOPMENT           "/DNDEBUG /Ox /MT")
  set(CMAKE_CXX_FLAGS_RELEASE               "/DNDEBUG /Ox /MT /GL")
  set(CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "/DEBUG /INCREMENTAL")
  set(CMAKE_MODULE_LINKER_FLAGS_DEVELOPMENT "/DEBUG /INCREMENTAL")
  set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT    "/DEBUG /INCREMENTAL")
  set(CMAKE_STATIC_LINKER_FLAGS_RELEASE     "/LTCG:OFF")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE     "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /LTCG /MANIFEST:NO")
  set(CMAKE_MODULE_LINKER_FLAGS_RELEASE     "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /LTCG /MANIFEST:NO")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE        "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /LTCG /MANIFEST:NO")
endif()

if (WIN32)
  add_library(delayimp STATIC IMPORTED)
  set_target_properties(delayimp PROPERTIES IMPORTED_LOCATION "delayimp.lib")
endif()


###############################################################################
# LINUX-SPECIFIC SETTINGS
###############################################################################

if (LINUX)
  if (BUILDING_64BIT)
    set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS   "${CMAKE_CXX_FLAGS} -fPIC -Wl,--no-undefined")
  else()
    set(CMAKE_C_FLAGS     "-fPIC -m32")
    set(CMAKE_CXX_FLAGS   "-fPIC -Wl,--no-undefined -m32 -mfpmath=sse -march=native")
  endif()
endif()


###############################################################################
# MACOS-SPECIFIC SETTINGS
###############################################################################

set(CMAKE_OSX_ARCHITECTURES "x86_64")

function (add_bundle name)
  if (APPLE)
    add_library(${name} MODULE ${ARGN})
    set_target_properties(${name} PROPERTIES BUNDLE TRUE)
  endif()
endfunction()


###############################################################################
# ANDROID-SPECIFIC SETTINGS
###############################################################################

if (ANDROID)
  set(CMAKE_CXX_FLAGS_RELEASE   "-O3")
  set(CMAKE_SHARED_LINKER_FLAGS "-static-libstdc++")
endif()


###############################################################################
# PLATFORM FLAGS
###############################################################################

if (WIN32)
  add_definitions(-DIPL_OS_WINDOWS)
elseif (LINUX)
  add_definitions(-DIPL_OS_LINUX)
elseif (APPLE)
  add_definitions(-DIPL_OS_MACOSX)
elseif (ANDROID)
  add_definitions(-DIPL_OS_ANDROID)
endif()

if (ANDROID_ARM)
  add_definitions(-DIPL_CPU_ARMV7)
elseif (ANDROID_X86)
  add_definitions(-DIPL_CPU_X86)
elseif (ANDROID_X64)
  add_definitions(-DIPL_CPU_X64)
elseif (ANDROID_ARM64)
  add_definitions(-DIPL_CPU_ARM64)
elseif (APPLE)
  add_definitions(-DIPL_CPU_X64)
elseif (BUILDING_64BIT)
  add_definitions(-DIPL_CPU_X64)
else()
  add_definitions(-DIPL_CPU_X86)
endif()


###############################################################################
# VERSION STAMPING
###############################################################################

if (UPDATE_VERSION_STAMPS)
  message("Updating version stamps to v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}...")
  configure_file(${CMAKE_HOME_DIRECTORY}/src/native/steamaudio_unity_version.h.in ${CMAKE_HOME_DIRECTORY}/src/native/steamaudio_unity_version.h)
  configure_file(${CMAKE_HOME_DIRECTORY}/src/native/audioplugin_phonon.rc.in ${CMAKE_HOME_DIRECTORY}/src/native/audioplugin_phonon.rc)
  configure_file(${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/SteamAudio/Version.cs.in ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/SteamAudio/Version.cs)
  configure_file(${CMAKE_HOME_DIRECTORY}/doc/phonon_unity.md.in ${CMAKE_HOME_DIRECTORY}/doc/phonon_unity.md)
endif()


###############################################################################
# TARGETS
###############################################################################

add_subdirectory (src/native)
add_subdirectory (doc)