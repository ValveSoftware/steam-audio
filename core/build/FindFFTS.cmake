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

find_path(FFTS_INCLUDE_DIR
	NAMES 			ffts.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/ffts/include
)

find_library(FFTS_LIBRARY
	NAMES ffts ffts_static
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/ffts/lib/${IPL_BIN_SUBDIR}/release
)

find_library(FFTS_LIBRARY_DEBUG
	NAMES ffts ffts_static
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/ffts/lib/${IPL_BIN_SUBDIR}/debug
)

find_package_handle_standard_args(FFTS
	FOUND_VAR 		FFTS_FOUND
	REQUIRED_VARS 	FFTS_LIBRARY FFTS_INCLUDE_DIR
)

if (FFTS_FOUND)
	if (NOT TARGET FFTS::FFTS)
		add_library(FFTS::FFTS UNKNOWN IMPORTED)
		set_target_properties(FFTS::FFTS PROPERTIES IMPORTED_LOCATION ${FFTS_LIBRARY})
		if (FFTS_LIBRARY_DEBUG)
			set_target_properties(FFTS::FFTS PROPERTIES IMPORTED_LOCATION_DEBUG ${FFTS_LIBRARY_DEBUG})
		endif()
		target_include_directories(FFTS::FFTS INTERFACE ${FFTS_INCLUDE_DIR})
	endif()
endif()

mark_as_advanced(FFTS_INCLUDE_DIR FFTS_LIBRARY)
