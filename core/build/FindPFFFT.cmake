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

find_path(PFFFT_INCLUDE_DIR
	NAMES 			pffft.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/pffft/include
)

find_library(PFFFT_LIBRARY
	NAMES pffft
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/pffft/lib/${IPL_BIN_SUBDIR}/release
)

find_library(PFFFT_LIBRARY_DEBUG
	NAMES pffft
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/pffft/lib/${IPL_BIN_SUBDIR}/debug
)

find_package_handle_standard_args(PFFFT
	FOUND_VAR 		PFFFT_FOUND
	REQUIRED_VARS 	PFFFT_LIBRARY PFFFT_INCLUDE_DIR
)

if (PFFFT_FOUND)
	if (NOT TARGET PFFFT::PFFFT)
		add_library(PFFFT::PFFFT UNKNOWN IMPORTED)
		set_target_properties(PFFFT::PFFFT PROPERTIES IMPORTED_LOCATION ${PFFFT_LIBRARY})
		if (PFFFT_LIBRARY_DEBUG)
			set_target_properties(PFFFT::PFFFT PROPERTIES IMPORTED_LOCATION_DEBUG ${PFFFT_LIBRARY_DEBUG})
		endif()
		target_include_directories(PFFFT::PFFFT INTERFACE ${PFFFT_INCLUDE_DIR})
	endif()
endif()

mark_as_advanced(PFFFT_INCLUDE_DIR PFFFT_LIBRARY)
