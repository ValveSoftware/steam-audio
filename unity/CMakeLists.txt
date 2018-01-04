#
# Copyright (c) Valve Corporation. All rights reserved.
#

###############################################################################
# GLOBAL SETTINGS
###############################################################################

# We require CMake 3.0 or later.
cmake_minimum_required (VERSION 3.0)

# We require compilers that support C++11.
set (CMAKE_CXX_STANDARD 11)

# Define the project.
project (Phonon)

# Precompiled headers.
set (CMAKE_MODULE_PATH "${CMAKE_HOME_DIRECTORY}/build")
include (cotire)

# Include directories.
include_directories (include include/phonon)

# Detect whether we're building for a 32-bit or 64-bit target.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (BUILDING_64BIT TRUE)
else ()
    set (BUILDING_64BIT FALSE)
endif ()

# Do not use configuration-dependent subdirectories to store output files.
# Instead directly use the output directory as passed in by the Python script.
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

set (CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set (CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set (CMAKE_C_RESPONSE_FILE_LINK_FLAG "@")
set (CMAKE_CXX_RESPONSE_FILE_LINK_FLAG "@")


###############################################################################
# WINDOWS-SPECIFIC SETTINGS
###############################################################################

if (MSVC)
    set (CMAKE_CXX_FLAGS                        "/DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /W3 /EHsc /MP")
    set (CMAKE_CXX_FLAGS_DEBUG                  "/D_DEBUG /Od /RTC1 /MTd /Zi")
    set (CMAKE_CXX_FLAGS_DEVELOPMENT            "/DNDEBUG /Ox /MT /Zi")
    set (CMAKE_CXX_FLAGS_RELEASE                "/DNDEBUG /Ox /MT /Zi /GL")
    set (CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT  "/DEBUG /INCREMENTAL")
    set (CMAKE_MODULE_LINKER_FLAGS_DEVELOPMENT  "/DEBUG /INCREMENTAL")
    set (CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT     "/DEBUG /INCREMENTAL")
    set (CMAKE_STATIC_LINKER_FLAGS_RELEASE      "/LTCG:OFF")
    set (CMAKE_SHARED_LINKER_FLAGS_RELEASE      "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /LTCG /MANIFEST:NO")
    set (CMAKE_MODULE_LINKER_FLAGS_RELEASE      "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /LTCG /MANIFEST:NO")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE         "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /LTCG /MANIFEST:NO")
endif ()

# Set the default link directories.
if (${WIN32})
  if (${BUILDING_64BIT})
    link_directories (lib/windows-x64)
  else ()
    link_directories (lib/windows-x86)
  endif ()
endif ()

# Add support for delay-loading DLLs.
if (${WIN32})
    if (${BUILDING_64BIT})
        add_library (delayimp STATIC IMPORTED)
        set_target_properties (delayimp PROPERTIES IMPORTED_LOCATION "delayimp.lib")
    endif ()
endif ()


###############################################################################
# MACOS-SPECIFIC SETTINGS
###############################################################################

# For Mac OS X, always build universal binaries.
set (CMAKE_OSX_ARCHITECTURES "i386;x86_64")

# Adds a .bundle for Mac OS X.
function (add_bundle name)
  if (${APPLE})
    add_library (${name} MODULE ${ARGN})
    set_target_properties (${name} PROPERTIES BUNDLE TRUE)
  endif ()
endfunction ()


###############################################################################
# ANDROID-SPECIFIC SETTINGS
###############################################################################

# For Android, we need the Java compiler.
if (${ANDROID})
  find_package (Java)
endif ()

# Adds a .apk for Android, or an executable on any other platform. Useful for
# building either an .exe or an .apk from the same source files.
function (add_apk_or_executable name)

  if (${ANDROID})

    # Build a .so wrapper from the C++ source core, and put in the appropriate
    # location in the Gradle project.
    add_library (${name} SHARED ${ARGN})
    set_target_properties (${name} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY          "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/armeabi-v7a"
      LIBRARY_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/armeabi-v7a"
      LIBRARY_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/armeabi-v7a"
      LINK_FLAGS                        "-llog -landroid -static-libstdc++"
      LINK_FLAGS_DEBUG                  "-llog -landroid -static-libstdc++"
      LINK_FLAGS_RELEASE                "-llog -landroid -static-libstdc++"
    )

    # After the .so has been built, use Ant to build an .apk, and copy it to
    # the output directory.
    add_custom_command (TARGET ${name} POST_BUILD
      COMMAND gradlew --no-daemon ${name}:assembleDebug WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/android
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_HOME_DIRECTORY}/build/android/${name}/build/outputs/apk/${name}-debug.apk ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.apk
    )

  else ()

    add_executable (${name} ${ARGN})

  endif ()

endfunction ()

# Compiler flags.
if (${ANDROID})
  set (CMAKE_CXX_FLAGS                   "-march=armv7-a -mfloat-abi=softfp -mfpu=neon -std=c++14")
  set (CMAKE_CXX_FLAGS_RELEASE           "-O3")
  set (CMAKE_EXE_LINKER_FLAGS            "-llog -landroid")
  set (CMAKE_SHARED_LINKER_FLAGS         "-llog -landroid")
endif ()


###############################################################################
# PLATFORM FLAGS
###############################################################################

if (ANDROID)
    add_definitions (-DIPL_OS_ANDROID -DIPL_CPU_ARMV7)
elseif (APPLE)
    add_definitions (-DIPL_OS_MACOSX -DIPL_CPU_X86 -DIPL_CPU_X64)
else ()
    if (BUILDING_64BIT)
        add_definitions (-DIPL_CPU_X64)
    else ()
        add_definitions (-DIPL_CPU_X86)
    endif ()
    if (MSVC)
        add_definitions (-DIPL_OS_WINDOWS)
    else ()
        add_definitions (-DIPL_OS_LINUX)
    endif ()
endif ()


###############################################################################
# TARGETS
###############################################################################

add_subdirectory (src/native)
add_subdirectory (doc)