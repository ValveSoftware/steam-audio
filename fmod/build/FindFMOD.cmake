# Copyright 2017-2023 Valve Corporation. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

include(FindPackageHandleStandardArgs)

find_path(FMOD_INCLUDE_DIR
	NAMES 			fmod.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/include
	PATH_SUFFIXES 	fmod
)

find_package_handle_standard_args(FMOD
	FOUND_VAR 		FMOD_FOUND
	REQUIRED_VARS 	FMOD_INCLUDE_DIR
)

# todo: version

if (FMOD_FOUND)
	if (NOT TARGET FMOD::FMOD)
		add_library(FMOD INTERFACE)
		target_include_directories(FMOD INTERFACE ${FMOD_INCLUDE_DIR})

		add_library(FMOD::FMOD ALIAS FMOD)
	endif()
endif()

mark_as_advanced(FMOD_INCLUDE_DIR)
