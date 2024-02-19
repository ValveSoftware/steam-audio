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

get_bin_subdir(IPL_BIN_SUBDIR)

find_path(PortAudio_INCLUDE_DIR
	NAMES 			portaudio.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/portaudio/include
)

find_library(PortAudio_LIBRARY
	NAMES portaudio_static_x64
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/portaudio/lib/${IPL_BIN_SUBDIR}/release
)

find_library(PortAudio_LIBRARY_DEBUG
	NAMES portaudio_static_x64
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/portaudio/lib/${IPL_BIN_SUBDIR}/debug
)

find_package_handle_standard_args(PortAudio
	FOUND_VAR 		PortAudio_FOUND
	REQUIRED_VARS 	PortAudio_LIBRARY PortAudio_INCLUDE_DIR
)

if (PortAudio_FOUND)
	if (NOT TARGET PortAudio::PortAudio)
		add_library(PortAudio::PortAudio UNKNOWN IMPORTED)
		set_target_properties(PortAudio::PortAudio PROPERTIES IMPORTED_LOCATION ${PortAudio_LIBRARY})
		if (PortAudio_LIBRARY_DEBUG)
			set_target_properties(PortAudio::PortAudio PROPERTIES IMPORTED_LOCATION_DEBUG ${PortAudio_LIBRARY_DEBUG})
		endif()
		target_include_directories(PortAudio::PortAudio INTERFACE ${PortAudio_INCLUDE_DIR})
	endif()
endif()

mark_as_advanced(PortAudio_INCLUDE_DIR PortAudio_LIBRARY)
