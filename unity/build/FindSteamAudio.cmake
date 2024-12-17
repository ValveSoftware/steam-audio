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

include(FindPackageHandleStandardArgs)
include(SteamAudioHelpers)

get_local_lib_path(IPL_LIB_PATH)

find_path(SteamAudio_INCLUDE_DIR
	NAMES 			phonon.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/include
	PATH_SUFFIXES 	phonon
)

if (BUILD_SHARED_LIBS)
	find_library(SteamAudio_LIBRARY
		NAMES phonon
		PATHS ${IPL_LIB_PATH}
	)

	find_package_handle_standard_args(SteamAudio
		FOUND_VAR 		SteamAudio_FOUND
		REQUIRED_VARS 	SteamAudio_INCLUDE_DIR
	)
else()
	find_package_handle_standard_args(SteamAudio
		FOUND_VAR 		SteamAudio_FOUND
		REQUIRED_VARS 	SteamAudio_INCLUDE_DIR
	)
endif()

if (SteamAudio_FOUND)
	if (NOT TARGET SteamAudio::SteamAudio)
		if (BUILD_SHARED_LIBS)
			add_library(SteamAudio INTERFACE)
			target_include_directories(SteamAudio INTERFACE ${SteamAudio_INCLUDE_DIR})

			add_library(SteamAudio::SteamAudio ALIAS SteamAudio)
		else()
			add_library(SteamAudio::SteamAudio UNKNOWN IMPORTED)
			target_include_directories(SteamAudio::SteamAudio INTERFACE ${SteamAudio_INCLUDE_DIR})
		endif()
	endif()
endif()

mark_as_advanced(SteamAudio_INCLUDE_DIR SteamAudio_LIBRARY)
