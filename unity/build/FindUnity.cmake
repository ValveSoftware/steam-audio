# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

include(FindPackageHandleStandardArgs)

find_path(Unity_INCLUDE_DIR 
	NAMES 			AudioPluginInterface.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/include
	PATH_SUFFIXES 	unity5
)

find_program(Unity_EXECUTABLE
	NAMES Unity.exe
	PATHS ${Unity_EXECUTABLE_DIR}
)

find_package_handle_standard_args(Unity 
	FOUND_VAR 		Unity_FOUND 
	REQUIRED_VARS 	Unity_INCLUDE_DIR
)

if (Unity_FOUND)
	if (NOT TARGET Unity::NativeAudio)
		add_library(UnityNativeAudio INTERFACE)
		target_include_directories(UnityNativeAudio INTERFACE ${Unity_INCLUDE_DIR})

		add_library(Unity::NativeAudio ALIAS UnityNativeAudio)
	endif()
endif()

mark_as_advanced(Unity_INCLUDE_DIR)
