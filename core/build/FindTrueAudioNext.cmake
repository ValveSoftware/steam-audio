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
	find_path(TrueAudioNext_INCLUDE_DIR
		NAMES 			TrueAudioNext.h
		PATHS 			${CMAKE_HOME_DIRECTORY}/deps/trueaudionext/include
	)

	find_library(TrueAudioNext_gpuutilities_LIBRARY
		NAMES gpuutilities
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/trueaudionext/lib/${IPL_BIN_SUBDIR}/release
	)

	find_library(TrueAudioNext_tan_LIBRARY
		NAMES TrueAudioNext
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/trueaudionext/lib/${IPL_BIN_SUBDIR}/release
	)

	find_library(TrueAudioNext_gpuutilities_LIBRARY_DEBUG
		NAMES gpuutilities
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/trueaudionext/lib/${IPL_BIN_SUBDIR}/debug
	)

	find_library(TrueAudioNext_tan_LIBRARY_DEBUG
		NAMES TrueAudioNext
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/trueaudionext/lib/${IPL_BIN_SUBDIR}/debug
	)

	if (TrueAudioNext_INCLUDE_DIR)
		file(STRINGS ${TrueAudioNext_INCLUDE_DIR}/TrueAudioNext.h _TrueAudioNext_VERSION_MAJOR REGEX "^#define[\t ]+TAN_VERSION_MAJOR[\t ]+.*")
		file(STRINGS ${TrueAudioNext_INCLUDE_DIR}/TrueAudioNext.h _TrueAudioNext_VERSION_MINOR REGEX "^#define[\t ]+TAN_VERSION_MINOR[\t ]+.*")
		file(STRINGS ${TrueAudioNext_INCLUDE_DIR}/TrueAudioNext.h _TrueAudioNext_VERSION_PATCH REGEX "^#define[\t ]+TAN_VERSION_RELEASE[\t ]+.*")
		file(STRINGS ${TrueAudioNext_INCLUDE_DIR}/TrueAudioNext.h _TrueAudioNext_VERSION_TWEAK REGEX "^#define[\t ]+TAN_VERSION_BUILD[\t ]+.*")
		string(REGEX REPLACE "^#define[\t ]+TAN_VERSION_MAJOR[\t ]+([0-9]+).*" "\\1" TrueAudioNext_VERSION_MAJOR ${_TrueAudioNext_VERSION_MAJOR})
		string(REGEX REPLACE "^#define[\t ]+TAN_VERSION_MINOR[\t ]+([0-9]+).*" "\\1" TrueAudioNext_VERSION_MINOR ${_TrueAudioNext_VERSION_MINOR})
		string(REGEX REPLACE "^#define[\t ]+TAN_VERSION_RELEASE[\t ]+([0-9]+).*" "\\1" TrueAudioNext_VERSION_PATCH ${_TrueAudioNext_VERSION_PATCH})
		string(REGEX REPLACE "^#define[\t ]+TAN_VERSION_BUILD[\t ]+([0-9]+).*" "\\1" TrueAudioNext_VERSION_TWEAK ${_TrueAudioNext_VERSION_TWEAK})
		set(TrueAudioNext_VERSION ${TrueAudioNext_VERSION_MAJOR}.${TrueAudioNext_VERSION_MINOR}.${TrueAudioNext_VERSION_PATCH}.${TrueAudioNext_VERSION_TWEAK})
		set(TrueAudioNext_VERSION_STRING ${TrueAudioNext_VERSION})
	endif()

	find_package_handle_standard_args(TrueAudioNext
		FOUND_VAR 		TrueAudioNext_FOUND
		REQUIRED_VARS	TrueAudioNext_tan_LIBRARY TrueAudioNext_gpuutilities_LIBRARY TrueAudioNext_INCLUDE_DIR
		VERSION_VAR 	TrueAudioNext_VERSION
	)

	if (TrueAudioNext_FOUND)
		if (NOT TARGET TrueAudioNext::TrueAudioNext)
			add_library(TrueAudioNext::GPUUtilities UNKNOWN IMPORTED)
			set_target_properties(TrueAudioNext::GPUUtilities PROPERTIES IMPORTED_LOCATION ${TrueAudioNext_gpuutilities_LIBRARY})
			if (TrueAudioNext_gpuutilities_LIBRARY_DEBUG)
				set_target_properties(TrueAudioNext::GPUUtilities PROPERTIES IMPORTED_LOCATION_DEBUG ${TrueAudioNext_gpuutilities_LIBRARY_DEBUG})
			endif()

			add_library(TrueAudioNext::tan UNKNOWN IMPORTED)
			set_target_properties(TrueAudioNext::tan PROPERTIES IMPORTED_LOCATION ${TrueAudioNext_tan_LIBRARY})
			if (TrueAudioNext_tan_LIBRARY_DEBUG)
				set_target_properties(TrueAudioNext::tan PROPERTIES IMPORTED_LOCATION_DEBUG ${TrueAudioNext_tan_LIBRARY_DEBUG})
			endif()

			add_library(TrueAudioNext INTERFACE)
			target_include_directories(TrueAudioNext INTERFACE ${OpenCL_INCLUDE_DIR} ${TrueAudioNext_INCLUDE_DIR})
			target_link_libraries(TrueAudioNext INTERFACE OpenCL::OpenCL TrueAudioNext::GPUUtilities TrueAudioNext::tan)

			add_library(TrueAudioNext::TrueAudioNext ALIAS TrueAudioNext)
		endif()
	endif()

	mark_as_advanced(TrueAudioNext_INCLUDE_DIR TrueAudioNext_gpuutilities_LIBRARY TrueAudioNext_tan_LIBRARY)
endif()
