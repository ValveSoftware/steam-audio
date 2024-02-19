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

set(OpenCL_ROOT ${CMAKE_HOME_DIRECTORY}/deps/opencl)
find_package(OpenCL)

if (OpenCL_FOUND)
	find_path(RadeonRays_INCLUDE_DIR
		NAMES 			radeon_rays.h
		PATHS 			${CMAKE_HOME_DIRECTORY}/deps/radeonrays/include
	)

	find_library(RadeonRays_clw_LIBRARY
		NAMES clw clw64
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/radeonrays/lib/${IPL_BIN_SUBDIR}/release
	)

	find_library(RadeonRays_calc_LIBRARY
		NAMES calc calc64
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/radeonrays/lib/${IPL_BIN_SUBDIR}/release
	)

	find_library(RadeonRays_rr_LIBRARY
		NAMES radeonrays radeonrays64
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/radeonrays/lib/${IPL_BIN_SUBDIR}/release
	)

	find_library(RadeonRays_clw_LIBRARY_DEBUG
		NAMES clwd clw64d
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/radeonrays/lib/${IPL_BIN_SUBDIR}/debug
	)

	find_library(RadeonRays_calc_LIBRARY_DEBUG
		NAMES calcd calc64d
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/radeonrays/lib/${IPL_BIN_SUBDIR}/debug
	)

	find_library(RadeonRays_rr_LIBRARY_DEBUG
		NAMES radeonraysd radeonrays64d
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/radeonrays/lib/${IPL_BIN_SUBDIR}/debug
	)

	if (RadeonRays_INCLUDE_DIR)
		file(STRINGS ${RadeonRays_INCLUDE_DIR}/radeon_rays.h _RadeonRays_VERSION REGEX "^#define[\t ]+RADEONRAYS_API_VERSION[\t ]+.*")
		string(REGEX REPLACE "^#define[\t ]+RADEONRAYS_API_VERSION[\t ]+([0-9])+\.([0-9]+).*" "\\1" RadeonRays_VERSION_MAJOR ${_RadeonRays_VERSION})
		string(REGEX REPLACE "^#define[\t ]+RADEONRAYS_API_VERSION[\t ]+([0-9])+\.([0-9]+).*" "\\2" RadeonRays_VERSION_MINOR ${_RadeonRays_VERSION})
		set(RadeonRays_VERSION ${RadeonRays_VERSION_MAJOR}.${RadeonRays_VERSION_MINOR})
		set(RadeonRays_VERSION_STRING ${RadeonRays_VERSION})
	endif()

	find_package_handle_standard_args(RadeonRays
		FOUND_VAR 		RadeonRays_FOUND
		REQUIRED_VARS	RadeonRays_rr_LIBRARY RadeonRays_calc_LIBRARY RadeonRays_clw_LIBRARY RadeonRays_INCLUDE_DIR
		VERSION_VAR 	RadeonRays_VERSION
	)

	if (RadeonRays_FOUND)
		if (NOT TARGET RadeonRays::RadeonRays)
			add_library(RadeonRays::clw UNKNOWN IMPORTED)
			set_target_properties(RadeonRays::clw PROPERTIES IMPORTED_LOCATION ${RadeonRays_clw_LIBRARY})
			if (RadeonRays_clw_LIBRARY_DEBUG)
				set_target_properties(RadeonRays::clw PROPERTIES IMPORTED_LOCATION_DEBUG ${RadeonRays_clw_LIBRARY_DEBUG})
			endif()

			add_library(RadeonRays::calc UNKNOWN IMPORTED)
			set_target_properties(RadeonRays::calc PROPERTIES IMPORTED_LOCATION ${RadeonRays_calc_LIBRARY})
			if (RadeonRays_calc_LIBRARY_DEBUG)
				set_target_properties(RadeonRays::calc PROPERTIES IMPORTED_LOCATION_DEBUG ${RadeonRays_calc_LIBRARY_DEBUG})
			endif()

			add_library(RadeonRays::rr UNKNOWN IMPORTED)
			set_target_properties(RadeonRays::rr PROPERTIES IMPORTED_LOCATION ${RadeonRays_rr_LIBRARY})
			if (RadeonRays_rr_LIBRARY_DEBUG)
				set_target_properties(RadeonRays::rr PROPERTIES IMPORTED_LOCATION_DEBUG ${RadeonRays_rr_LIBRARY_DEBUG})
			endif()

			add_library(RadeonRays INTERFACE)
			target_include_directories(RadeonRays INTERFACE ${RadeonRays_INCLUDE_DIR})
			target_link_libraries(RadeonRays INTERFACE OpenCL::OpenCL RadeonRays::clw RadeonRays::calc RadeonRays::rr)

			add_library(RadeonRays::RadeonRays ALIAS RadeonRays)
		endif()
	endif()

	mark_as_advanced(RadeonRays_INCLUDE_DIR RadeonRays_clw_LIBRARY RadeonRays_calc_LIBRARY RadeonRays_rr_LIBRARY)
endif()
